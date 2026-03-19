/**
 * Version 1.0.0
 * MakeSense IoT - ESP32 Blynk Soil Moisture Monitor
 *
 * Reads a capacitive soil moisture sensor and publishes the
 * percentage to Blynk Cloud. A relay-driven water pump can be
 * toggled remotely from the Blynk app.
 *
 * Blynk Virtual Pins:
 *   V0 — Soil moisture percentage (publish)
 *   V1 — Water pump relay control (subscribe)
 *
 * Hardware: ESP32 + Capacitive soil moisture sensor + Relay (water pump)
 *
 * Pin Mapping:
 *   GPIO35 — Soil moisture sensor (ADC input)
 *   GPIO15 — Relay output (water pump)
 *
 * Copyright (c) 2026 Roboshop Technologies Sdn Bhd. All rights reserved.
 */

// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID           "TMPxxxxxx"
#define BLYNK_TEMPLATE_NAME         "Device"
#define BLYNK_AUTH_TOKEN            "YourAuthToken"

#include <BlynkSimpleEsp32.h>

#define wifi_username "your wifi username"
#define wifi_password "your wifi password"

//pinout declaratiorn
#define SENSORPIN 35 //soil moisture sensor
#define RELAY 15 //waterpump

#define VIRTUALPIN_RELAY    V1 

BlynkTimer timer;


BLYNK_WRITE(VIRTUALPIN_RELAY) {
  int RelayState = param.asInt();
  digitalWrite(RELAY,RelayState);
}

void sendSensor()
{
  int sensor_reading = analogRead(SENSORPIN);
  int map_reading = map(sensor_reading,1800,4095,100,0);

  Blynk.virtualWrite(V0, map_reading);
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAY,OUTPUT);
  Blynk.begin(BLYNK_AUTH_TOKEN, wifi_username, wifi_password);
  timer.setInterval(1000L, sendSensor);
}

void loop() {
  Blynk.run();
  timer.run();
}
