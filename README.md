# MakeSense IoT Kit

A collection of Arduino firmware examples for ESP32 and ESP8266 microcontrollers, designed for IoT education and rapid prototyping. Built around two IoT platforms: **Blynk 2.0** for drag-and-drop cloud dashboards and **MQTT** for lightweight pub/sub messaging.

Developed by **Roboshop Technologies Sdn Bhd**.

---

## Repository Structure

| # | Folder | MCU | Platform | Sensor / Actuator | Description |
|---|--------|-----|----------|-------------------|-------------|
| 01 | `01-BLYNK_2.0_ANALOG_GAS` | ESP32 | Blynk Edgent | MQ-series gas sensor | Gas monitoring with Blynk Edgent (OTA, provisioning, captive portal) |
| 02 | `02-BLYNK_2.0_WITHOUT_EDGENT_ANALOG` | ESP32 | Blynk IoT | MQ-series gas sensor | Simplified gas monitoring using `BlynkSimpleEsp32` (no Edgent) |
| 03 | `03-Blynk_IoT_SoilMoisture` | ESP32 | Blynk IoT | Capacitive soil moisture + relay | Soil moisture reading with remote water pump control via Blynk |
| 04 | `04-MQTT_4CHRELAY_FARMSENSENSE` | ESP8266 | MQTT (PubSubClient) | 4-channel relay module | FarmSense 4-channel relay controller with JSON commands, safety timers, cooldown, and WiFiManager |
| 05 | `05-MQTT_GAS_SENSOR` | ESP32 | MQTT | MQ-series gas sensor | Gas value published to MQTT broker with WiFiManager captive portal |
| 06 | `06-MQTT_PH` | ESP32 | MQTT | DFRobot pH sensor + 16x2 LCD | pH monitoring with I2C LCD display and MQTT publishing |
| 07 | `07-MQTT_SOIL_FARMSENSE` | ESP32 | MQTT | Capacitive soil moisture | FarmSense soil moisture node with WiFiManager, JSON data, heartbeat, and remote config |
| 08 | `08-MQTT_SOIL_MOISTURE` | ESP32 | MQTT | Capacitive soil moisture | Basic soil moisture to MQTT with WiFiManager |
| 09 | `09-PUBSUBCLIENT_SMARTHOME` | ESP32 | MQTT (PubSubClient) | DHT11 temp/humidity + LED | Smart home sensor node: publishes temperature and humidity, subscribes for LED control |
| 10 | `10-PUBSUBCLIENT_SMARTHOME_OLED` | ESP32 | MQTT (PubSubClient) | DHT11 + OLED + relay + buzzer | Extended smart home: OLED display, relay control, buzzer, and LED via MQTT |

---

## Hardware Requirements

### Microcontrollers
- **ESP32 DevKit** (projects 01-03, 05-10)
- **ESP8266** (project 04)

### Sensors & Modules
| Component | Used In |
|-----------|---------|
| MQ-series analog gas sensor | 01, 02, 05 |
| Capacitive soil moisture sensor | 03, 07, 08 |
| DHT11 temperature & humidity sensor | 09, 10 |
| DFRobot analog pH sensor (PH_PIN) | 06 |
| 4-channel relay module | 04 |
| Single relay module | 03, 10 |
| 16x2 I2C LCD (address 0x27) | 06 |
| 0.96" I2C OLED SSD1306 (128x64) | 10 |
| Buzzer | 06, 10 |
| Red & green LEDs | 01, 02, 05 |

---

## Software Requirements

### Arduino IDE Setup
1. Install [Arduino IDE](https://www.arduino.cc/en/software) (1.8.x or 2.x)
2. Add ESP32 & ESP8266 board support:
   - **File > Preferences > Additional Board Manager URLs:**
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - **Tools > Board > Boards Manager** — install *esp32* and *esp8266*

### Required Libraries

Install via **Sketch > Include Library > Manage Libraries**:

| Library | Used By |
|---------|---------|
| `Blynk` (by Volodymyr Shymanskyy) | 01, 02, 03 |
| `PubSubClient` (by Nick O'Leary) | 04, 07, 09, 10 |
| `MQTT` (by Joel Gaehwiler) | 05, 06, 08 |
| `WiFiManager` (by tzapu) | 04, 05, 06, 07, 08 |
| `ArduinoJson` (by Benoit Blanchon) | 04, 07, 09, 10 |
| `DHT sensor library` (by Adafruit) | 09, 10 |
| `Adafruit Unified Sensor` | 09, 10 |
| `Adafruit SSD1306` | 10 |
| `Adafruit GFX Library` | 10 |
| `LiquidCrystal I2C` | 06 |
| `DFRobot_ESP_PH` | 06 |

---

## Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/arifroboshoptech/MakeSense_IoT.git
```

### 2. Open a Project
Open the `.ino` file from any project folder in Arduino IDE.

### 3. Configure Credentials
Each project requires WiFi and/or platform credentials to be set before uploading:

**Blynk projects (01-03):**
- Set `BLYNK_TEMPLATE_ID`, `BLYNK_TEMPLATE_NAME`, and `BLYNK_AUTH_TOKEN` from your [Blynk Console](https://blynk.cloud)
- Set `wifi_username` and `wifi_password` (projects 02 & 03)

**MQTT projects (04-10):**
- Default broker: `broker.hivemq.com` (public, no auth required)
- Change `MQTT_PREFIX_TOPIC` or `MQTT_PREFIX` to a unique value to avoid topic collisions
- Projects with WiFiManager (04-08) use a captive portal — connect to the AP and configure WiFi via browser

### 4. Select Board & Upload
- **Tools > Board**: Select `ESP32 Dev Module` or `NodeMCU 1.0 (ESP-12E)` as appropriate
- **Tools > Port**: Select the correct COM port
- Click **Upload**

---

## MQTT Topic Reference

### Basic Projects (05, 06, 08)
```
{prefix}/mqtt/data       — Sensor readings (publish)
{prefix}/mqtt/control    — Commands (subscribe)
```

### FarmSense Soil Sensor (07)
```
farm/sensors/{deviceId}/data     — Soil moisture JSON (publish)
farm/sensors/{deviceId}/status   — Heartbeat (publish)
farm/sensors/{deviceId}/config   — Remote config (subscribe)
```

### FarmSense 4CH Relay (04)
```
farm/actuators/{deviceId}_chN/command    — ON/OFF/TOGGLE (subscribe)
farm/actuators/{deviceId}_chN/state      — Relay state JSON (publish)
farm/actuators/{deviceId}_chN/status     — Heartbeat (publish)
farm/actuators/{deviceId}_chN/response   — Command acknowledgement (publish)
```

### Smart Home (09, 10)
```
Tempdata    — Temperature,Humidity CSV string (publish)
lights      — ON/OFF for LED control (subscribe)
RelayHome1  — ON/OFF for relay control (subscribe, project 10 only)
```

---

## Pin Mapping Quick Reference

### Blynk Gas Monitoring (01, 02)
| Pin | Function |
|-----|----------|
| GPIO32 | Analog gas sensor input |
| GPIO23 | Red LED |
| GPIO22 | Green LED |

### Blynk Soil Moisture (03)
| Pin | Function |
|-----|----------|
| GPIO35 | Soil moisture sensor |
| GPIO15 | Water pump relay |

### FarmSense 4CH Relay — ESP8266 (04)
| Pin | Function |
|-----|----------|
| GPIO16 | Relay CH1 |
| GPIO14 | Relay CH2 |
| GPIO12 | Relay CH3 |
| GPIO13 | Relay CH4 |
| GPIO15 | WiFi config button |

### FarmSense Soil Sensor (07)
| Pin | Function |
|-----|----------|
| GPIO35 | Soil moisture sensor |
| GPIO2 | Config button / LED |

### Smart Home (09, 10)
| Pin | Function |
|-----|----------|
| GPIO4 | DHT11 data |
| GPIO17 | LED |
| GPIO16 | Relay (project 10) |
| GPIO13 | Buzzer (project 10) |

---

## Documentation

Additional documentation and resources are located in the `00-Documentation/` folder:
- **Documentation MakeSense IoT Kit.pdf** — Full kit documentation
- **MIT Apps Inventor extension** — MQTT extension (UrsAI2Paho) and sample `.aia` project for building Android MQTT dashboards

---

## License

Copyright (c) Roboshop Technologies Sdn Bhd. All rights reserved.
