/***************************************************************************************************
* userApp.c
*
*  Created on: Oct 3, 2025
*  Author: Niall.OKeeffe@atu.ie
*
* Demo application to connect to a cloud based MQTT broker
* Data communication is un-encrypted using port 1883
*
* Before running the code: -
* 1. open /Core/Inc/WiFi_Credentials.h and enter WiFi access point SSID and password
* 2. open /Core/Inc/CloudBrokerCredentials.h and enter your broker credentials
* 3. Change the publish topic in the userApp() while(1) loop
* 4. Change the subscribe topic in the userApp() function
* 5. Change the subscribe topic in the subscribeMessageHandler() function
*
*
* Application Functionality
* 1. Fetches the epoch time from st.com and uses it to start the RTC with a 1-second wake-up interrupt
* 2. Connects to wireless access point and then to cloud DNS
* 3. Connects to MQTT broker
* 4. Subscribes to a single topic
* 5. Prints date and time every second
* 6. Publishes to a topic every time button is pressed
* 7. Subscribe callback function runs every time a publish message is received from the broker
********************************************************************************************************
 *
 */

#include "main.h"
#include "userApp.h"
#include "stm32l475e_iot01_tsensor.h"
#include <stdlib.h>
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim2;

extern int network_wr(Network* n, unsigned char* buffer, int len, int timeout_ms);
extern int network_rd(Network* n, unsigned char* buffer, int len, int timeout_ms);
extern int net_if_init(void * if_ctxt);
extern int net_if_deinit(void * if_ctxt);
extern int net_if_reinit(void * if_ctxt);
extern int wifi_net_if_init(void * if_ctxt);

uint8_t timeDisplay = 0, publishTemperature = 0, publishHumidity = 0, publishPressure = 0, systemStatus = 0;
uint8_t keyIndex = 0;
uint8_t lock = 0;
uint8_t key[5] = {1, 2, 3, 4};
uint8_t enteredKey[5];
uint8_t dutyCycle = 0;
net_hnd_t hnet;
Network network;
MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
net_sockhnd_t socket;


typedef struct {
  char *HostName;
  char *HostPort;
  char *ConnSecurity;
  char *MQClientId;
  char *MQUserName;
  char *MQUserPwd;
#ifdef LITMUS_LOOP
  char *LoopTopicId;
#endif
} device_config_t;

/*------------------------------------------------------------------
 * RTC timer event callback function
 * Runs every second
 ------------------------------------------------------------------*/
void HAL_RTCEx_WakeUpTimerEventCallback (RTC_HandleTypeDef * hrtc) {
	timeDisplay = 1;
}



void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
	case (GPIO_PIN_1):
	{
		SPI_WIFI_ISR();
		break;
	}
	//Publishing the pressure every button press
	case (BUTTON_EXTI13_Pin) :
	{
		printf("User button pressed\r\n\n");
		printf("Publishing pressure\r\n\n");
		publishPressure = 1;
	}

    default:
    {
      break;
    }
  }
}

//TIM Interrupt callback
void HAL_TIM_PeriodElapsedCallback (TIM_HandleTypeDef * htim) {
	if(htim->Instance == TIM7) {
		publishTemperature = 1;
		publishHumidity = 1;
	}
}

/*--------------------------------------------------------------------------
 * Subscribe message callback function
 * Called every time a publish meassage is received from a subscribed topic
 --------------------------------------------------------------------------*/
void subscribeMessageHandler(MessageData* data)
{
	static char mqtt_msg[MQTT_MSG_BUFFER_SIZE], mqtt_topic[MQTT_TOPIC_BUFFER_SIZE];
	snprintf(mqtt_msg, data->message->payloadlen+1, "%s", (char *)data->message->payload);
	snprintf(mqtt_topic, data->topicName->lenstring.len+1, "%s", data->topicName->lenstring.data);
	printf("\r\nPublished message from MQTT broker\r\n");
	printf("Topic: %s, Payload: %s\r\n\n", mqtt_topic, mqtt_msg);
	// LED Control
	if(strstr(mqtt_topic, "LanceCruz/feeds/led-control")) {	//edit username and feed name
		//add code here to parse the string mqtt_msg. This is the message payload.
		if(lock == 1) {
			if(strstr(mqtt_msg, "ON")) {
				HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_PIN, 1);
			}
			else if(strstr(mqtt_msg, "OFF")) {
				HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_PIN, 0);
			}
		}
		else {
			printf("Please enter the code to unlock the lights control.\r\n\n");
		}
	}
	// Motor Control using a slider that increments every 10
	if(strstr(mqtt_topic, "LanceCruz/feeds/motor-control")) {
		uint8_t speed = atoi(mqtt_msg);
		//TIM2->CCR3 = ((TIM2->ARR+1)/100)*dutyCycle;#
		if(lock == 1) {
			if (speed == 0) {
				HAL_GPIO_WritePin (IN1_GPIO_Port,IN1_Pin,0);
				HAL_GPIO_WritePin (IN2_GPIO_Port,IN2_Pin,0);
			}
			else if (speed >= 10) {
				dutyCycle = speed;
				HAL_GPIO_WritePin (IN1_GPIO_Port,IN1_Pin,1);
				HAL_GPIO_WritePin (IN2_GPIO_Port,IN2_Pin,0);
				TIM2->CCR3 = ((TIM2->ARR+1)/100)*dutyCycle;
			}
		}
		else {
			printf("Please enter the code to unlock the fan control.\r\n\n");
		}
	}
	// Keypad subscribe that locks and unlocks the system
	// systemStatus = 1
	if (strstr(mqtt_topic, "LanceCruz/feeds/keypad"))
	{
		uint8_t pressedKey = atoi(mqtt_msg);

		printf("Keypad key received: %d\r\n", pressedKey);

		enteredKey[keyIndex] = pressedKey;
		keyIndex++;

		if(keyIndex == 4) {
			uint8_t passwordCount = 0;

			for(uint8_t i = 0; i < 4; i++) {
				if(enteredKey[i] == key[i]) {
					passwordCount++;
				}
			}

			if(passwordCount == 4) {
				printf("System UNLOCKED\r\n");

				lock = 1;
				systemStatus = 1;
				keyIndex = 0;
			}
			else {
				printf("Wrong key. Reset keypad.\r\n");

				lock = 0;
				systemStatus = 1;
				keyIndex = 0;
			}
		}
	}
}


void userApp() {
	char temperatureMsg[25];
	//uint8_t temperature;
	int32_t ret;
	char humidityMsg[25];
	char pressureMsg[25];
	char unlockMsg[25];

	//Network and MQTT variables
	MQTTMessage mqmsg;
	MQTTClient client;

	//RTC variables
	//RTC_TimeTypeDef sTime;
	//RTC_DateTypeDef sDate;
	//char timeBuffer[40];

	printf("Starting user application\r\n\n");

	//LED off at initialisation
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

	//Connect to Adafriot IO MQTT broker
	brokerConnect(&client);

	//Add code here to subscribe to topics
	ret = MQTTSubscribe(&client, "LanceCruz/feeds/led-control", QOS0, (subscribeMessageHandler));	//edit username and feed name
	if (ret != MQSUCCESS) {
		printf("\n\rSubscribe to led-control failed: %ld\r\n", ret);
	}
	else {
		printf("\n\rSubscribed to led-control\r\n");	//edit feedname
		ret = MQTTYield(&client, 500);
	}
	ret = MQTTSubscribe(&client, "LanceCruz/feeds/motor-control", QOS0, (subscribeMessageHandler));	//edit username and feed name
		if (ret != MQSUCCESS) {
			printf("\n\rSubscribe to motor-control failed: %ld\r\n", ret);
		}
		else {
			printf("\n\rSubscribed to motor-control\r\n");	//edit feedname
			ret = MQTTYield(&client, 500);
		}
	ret = MQTTSubscribe(&client, "LanceCruz/feeds/keypad", QOS0, (subscribeMessageHandler));	//edit username and feed name
		if (ret != MQSUCCESS) {
			printf("\n\rSubscribe to keypad failed: %ld\r\n", ret);
		}
		else {
			printf("\n\rSubscribed to keypad\r\n");	//edit feedname
			ret = MQTTYield(&client, 500);
		}
	HAL_Delay(1000);
	BSP_TSENSOR_Init();
	BSP_HSENSOR_Init();
	BSP_PSENSOR_Init();

	__HAL_TIM_CLEAR_IT(&htim7, TIM_IT_UPDATE);
	HAL_NVIC_GetPendingIRQ(TIM7_IRQn);
	HAL_TIM_Base_Start_IT(&htim7);

	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);


	while(1) {
		ret = MQTTYield(&client, 500);	//check for published messages from cloud - do not remove!

		/*
		 * Display date/time every second
		 */
		/*if(timeDisplay) {
			timeDisplay = 0;
			HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
			HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
			sprintf(timeBuffer, "%02d/%02d/%02d %02d:%02d:%02d\r\n", sDate.Date, sDate.Month, sDate.Year, sTime.Hours+1, sTime.Minutes, sTime.Seconds);
			printf("%s", timeBuffer);
		}*/

		/*
		 * Read sensor every button EXTI interrupt
		 * Uses a random number to simulate a temperature between 1 and 30
		 */
		if(publishTemperature) {
			if(lock == 1) {
				publishTemperature = 0;
				
				float tempF = BSP_TSENSOR_ReadTemp();
				uint16_t tempI = tempF * 10;

				//create publish message
				sprintf(temperatureMsg, "%d.%d", tempI/10, tempI%10);
				printf("Temperature: %sC\r\n", temperatureMsg);
				memset(&mqmsg, 0, sizeof(MQTTMessage));
				mqmsg.qos = QOS0;
				mqmsg.payload = (char *) temperatureMsg;
				mqmsg.payloadlen = strlen(temperatureMsg);

				MQTTPublish(&client, "LanceCruz/feeds/temperature", &mqmsg);	//edit username and feedname
				printf("Publishing temperature: %dC\r\n\n", tempI/10);
			}
			else {
				printf("System tried to publish temperature.\r\n");
				publishTemperature = 0;
			}
		}

		if(publishHumidity == 1) {
			if(lock == 1) {
				publishHumidity = 0;

				float humdF = BSP_HSENSOR_ReadHumidity();
				uint16_t humdI = humdF * 10;

				//create publish message
				sprintf(humidityMsg, "%d.%d", humdI/10, humdI%10);
				printf("Humidity: %s\r\n", humidityMsg);
				memset(&mqmsg, 0, sizeof(MQTTMessage));
				mqmsg.qos = QOS0;
				mqmsg.payload = (char *) humidityMsg;
				mqmsg.payloadlen = strlen(humidityMsg);

				MQTTPublish(&client, "LanceCruz/feeds/humidity", &mqmsg);
				printf("Publishing humidity: %d\r\n\n", humdI/10);
			}
			else {
				printf("System tried to publish humidity.\r\n");
				printf("Please enter the code to unlock the system\r\n\n");
				publishHumidity = 0;
			}
		}

		if(publishPressure) {
			if(lock == 1) {
				publishPressure = 0;
				float presF = BSP_PSENSOR_ReadPressure();
				uint16_t presI = presF * 10;

				//create publish message
				sprintf(pressureMsg, "%d.%d", presI/10, presI%10);
				printf("Pressure: %smBar\r\n", pressureMsg);
				memset(&mqmsg, 0, sizeof(MQTTMessage));
				mqmsg.qos = QOS0;
				mqmsg.payload = (char *) pressureMsg;
				mqmsg.payloadlen = strlen(pressureMsg);

				MQTTPublish(&client, "LanceCruz/feeds/pressure", &mqmsg);
				printf("Publishing pressure: %dmBar\r\n", presI/10);
			}
			else {
				printf("System is locked please enter the code to unlock.\r\n\n");
				publishPressure = 0;
			}
		}

		if(systemStatus == 1){
    		systemStatus = 0;

			sprintf(unlockMsg, "%d", lock);
			memset(&mqmsg, 0, sizeof(MQTTMessage));
			mqmsg.qos = QOS0;
			mqmsg.payload = unlockMsg;
			mqmsg.payloadlen = strlen(unlockMsg);

			MQTTPublish(&client, "LanceCruz/feeds/system-status", &mqmsg);
			printf("Publishing system status: %s\r\n\n", unlockMsg);
		}

	}
}

void brokerConnect(MQTTClient * client) {
	int32_t ret;
	//Network and MQTT variables
	device_config_t MQTT_Config;
	static unsigned char mqtt_send_buffer[MQTT_SEND_BUFFER_SIZE];
	static unsigned char mqtt_read_buffer[MQTT_READ_BUFFER_SIZE];
	net_ipaddr_t ipAddr;
	net_macaddr_t macAddr;

	//Added
	//bool b_mqtt_connected           = false;
	//const char * connectionString   = NULL;
	//device_config_t * device_config = NULL;
#ifdef USE_MBED_TLS
	conn_sec_t connection_security  = CONN_SEC_UNDEFINED;
	const char * device_cert  = NULL;
	const char * device_key   = NULL;
#endif

	//Initialise MQTT broker structure
	//Fill in this section with MQTT broker credentials from header file
	MQTT_Config.HostName = CloudBroker_HostName;
	MQTT_Config.HostPort = CloudBroker_Port;
	MQTT_Config.ConnSecurity = "0";	//plain TCP connection with no security
	MQTT_Config.MQUserName = CloudBroker_Username;
	MQTT_Config.MQUserPwd = CloudBroker_Password;
	MQTT_Config.MQClientId = CloudBroker_ClientID;

	//Initialise WiFi network
	if (net_init(&hnet, NET_IF, (wifi_net_if_init)) != NET_OK) {
	//if (net_init(&hnet, NET_IF, (net_if_init)) != NET_OK) {
		printf("\n\rError");
	}
	else {
		printf("\n\rOK");
	}
	HAL_Delay(500);

	printf("\n\rRetrieving the IP address.");

	if (net_get_ip_address(hnet, &ipAddr) != NET_OK) {
		printf("\n\rError 2");
	}
	else
	{
		switch(ipAddr.ipv) {
			case NET_IP_V4:
				printf("\n\rIP address: %d.%d.%d.%d\n\r", ipAddr.ip[12], ipAddr.ip[13], ipAddr.ip[14], ipAddr.ip[15]);
				break;
			case NET_IP_V6:
			default:
				printf("\n\rError 3");
		}
	}

	if (net_get_mac_address(hnet, &macAddr) == NET_OK) {
		printf("\n\rMac Address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
	               macAddr.mac[0], macAddr.mac[1], macAddr.mac[2], macAddr.mac[3], macAddr.mac[4], macAddr.mac[5]);
	}

	/*
	* Fetch the epoch time from st.com and use it to set the RTC time
	*/
	if (setRTCTimeDateFromNetwork(true) != TD_OK) {
		printf("Fail setting time\r\n");
	}
	else {
		printf("Time set, Starting RTC\r\n");
		//RTC started with a 1-second wake-up interrupt
		HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 2047, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
	}
#ifdef USE_MBED_TLS
	printf("Connecting to MQTT Broker Server using TLS\r\n\n");
	//Create network socket
	//ret = net_sock_create(hnet, &socket, NET_PROTO_TCP);
	connection_security = CONN_SEC_SERVERAUTH;
	//ret = net_sock_create(hnet, &socket, (connection_security == CONN_SEC_NONE) ? NET_PROTO_TCP :NET_PROTO_TLS);
	ret = net_sock_create(hnet, &socket, NET_PROTO_TLS);
#else
	printf("Connecting to MQTT Broker Server\r\n\n");
	ret = net_sock_create(hnet, &socket, NET_PROTO_TCP);
#endif
	if (ret != NET_OK)
	{
		printf("\n\rCould not create the socket.\r\n");
		printf("Check MQTT broker configuration settings.\r\n");
		while(1);
	}
	else
	{
#ifdef USE_MBED_TLS
		//ret |= net_sock_setopt(socket, "sock_noblocking", NULL, 0);
        switch(connection_security)
        {
          case CONN_SEC_MUTUALAUTH:
            ret |= ((checkTLSRootCA() != 0) && (checkTLSDeviceConfig() != 0) )
              || (getTLSKeys(&ca_cert, &device_cert, &device_key) != 0);
            ret |= net_sock_setopt(socket, "tls_server_name", (void *) CloudBroker_HostName, strlen(CloudBroker_HostName) + 1);
            ret |= net_sock_setopt(socket, "tls_ca_certs",    (void *) ca_cert,                 strlen(ca_cert) + 1);
            ret |= net_sock_setopt(socket, "tls_dev_cert",    (void *) device_cert,             strlen(device_cert) + 1);
            ret |= net_sock_setopt(socket, "tls_dev_key",     (void *) device_key,              strlen(device_key) + 1);
            break;
          case CONN_SEC_SERVERNOAUTH:
            ret |= net_sock_setopt(socket, "tls_server_noverification", NULL, 0);
            ret |= (checkTLSRootCA() != 0)
              || (getTLSKeys(&ca_cert, NULL, NULL) != 0);
            ret |= net_sock_setopt(socket, "tls_server_name", (void *) CloudBroker_HostName, strlen(CloudBroker_HostName) + 1);
            ret |= net_sock_setopt(socket, "tls_ca_certs",    (void *) ca_cert,                 strlen(ca_cert) + 1);
              break;
          case CONN_SEC_SERVERAUTH:
            ret |= net_sock_setopt(socket, "tls_server_name", (void *) CloudBroker_HostName, strlen(CloudBroker_HostName) + 1);
            ret |= net_sock_setopt(socket, "tls_ca_certs",    (void *) ca_cert,                 strlen(ca_cert) + 1);
            break;
          case CONN_SEC_NONE:
            break;
          default:
            msg_error("Invalid connection security mode. - %d\n", connection_security);
        }
#endif
        ret |= net_sock_setopt(socket, "sock_noblocking", NULL, 0);
        if (ret != NET_OK)
		{
        	printf("Could not retrieve the security connection settings and set the socket options.\n");
        	while(1);
		}
		else {


			ret = net_sock_open(socket, MQTT_Config.HostName, atoi(CloudBroker_Port), 0);
			if (ret != NET_OK)
			{
				printf("\n\rCould not open the socket.");
				while(1);
			}
			else {
#ifdef USE_MBED_TLS
				printf("\r\nTLS connection established to MQTT Broker Server\r\n\n");
#else
				printf("\r\nConnection established to MQTT Broker Server\r\n\n");
#endif
				HAL_Delay(1000);
			}

			network.my_socket = socket;
			network.mqttread = (network_rd);
			network.mqttwrite = (network_wr);

			MQTTClientInit(client, &network, MQTT_CMD_TIMEOUT, mqtt_send_buffer, MQTT_SEND_BUFFER_SIZE,
					mqtt_read_buffer, MQTT_READ_BUFFER_SIZE);

			/* MQTT connect */
			options.clientID.cstring = MQTT_Config.MQClientId;
			options.username.cstring = MQTT_Config.MQUserName;
			options.password.cstring = MQTT_Config.MQUserPwd;

			HAL_Delay(1000);

			printf("Connecting client to MQTT Broker\r\n\n");
			ret = MQTTConnect(client, &options);
			if (ret != 0)
			{
				printf("\n\rMQTTConnect() failed: %ld\n", ret);
				printf("Check MQTT client credential settings.\r\n");
				while(1);
			}
			else
			{
				printf("\n\rClient Connected to MQTT Broker\r\n");
				HAL_Delay(1000);
			}
		}
        HAL_Delay(1000);
	}
}
