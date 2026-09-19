# AegisFlow ESP32 Upgraded

An upgraded ESP32 IoT sensor project that communicates over MQTT.

## Overview
This firmware connects an ESP32 to a Wi-Fi network and streams sensor data (DHT11 temperature/humidity and MQ-x gas sensor) to an EMQX MQTT broker. It also includes remote relay control.

## Hardware
- ESP32 Microcontroller
- DHT11 Temperature & Humidity Sensor
- Gas Sensor (Analog)
- Relay Module

## Configuration
Before running, you must configure:
1. `WIFI_SSID` and `WIFI_PASSWORD` in the source file.
2. `MQTT_BROKER` and `MQTT_PORT`.

## Security Warning
**Do not hardcode your actual Wi-Fi passwords in the firmware before committing to a public repository!** Use a separate header file (e.g., `secrets.h`) added to `.gitignore`.
