# EmbeddedSystems-Project
This is a Third Year Embedded Systems Project where we were assigned to develop an Internet of Things embedded system project with an STM32 microcontroller.

## Project Summary 
This project is a Smart Home system and visualized through an Adafruit IO dashboard. The system includes a keypad to lock or unlock access, with an indicator showing the current state. A slider controls a motor to simulate a fan, a button toggles an LED, a gauge displays pressure readings whenever the pressure button is pressed, and two line charts show temperature and humidity values published periodically. All interactions and sensor data are displayed in real time on the [dashboard](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/adafruitDashboard.png).

This was developed using the provided boilerplate code as a starting point. The primary development work was carried out in the C source files [userapp.c](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c) and [main.c](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/main.c), where the core application functionality and system behaviour were implemented and modified.

## Features

**Subscribed Commands**

* LED Control – Turns the onboard LED on or off based on dashboard commands. [[1]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/edit/main/userApp.c#L126)
* Motor Control – Uses a slider to simulate fan speed using PWM (PA2 – EN, PC1 – IN1, PC2 – IN2). [[2]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L162)
* Keypad Unlock – Requires the correct keypad code before any subscribed features can be used. [[3]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L162)

**Published Data**

* Temperature – Published on every timer interrupt event. [[4]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L279)
* Humidity – Published together with temperature on each timer interrupt. [[5]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L303)
* Pressure – Published whenever the pressure button (GPIO interrupt) is pressed. [[6]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L328)
* Unlock Status – Updates based on keypad state (locked or unlocked). [[7]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/userApp.c#L351)

**STM32 Interrupt Sources**

* GPIO Interrupt – Triggers pressure publishing when the button is pressed. [[8]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/main.c#L515)
* Timer Interrupt – Periodically reads and publishes temperature and humidity. [[9]](https://github.com/Lance-Cruz/EmbeddedSystems-Project/blob/main/main.c#L572)
