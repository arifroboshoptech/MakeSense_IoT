/**
 * Version 1.0.0
 * MakeSense IoT - ESP32 Blynk Gas Monitoring (Without Edgent)
 *
 * Simplified version of the gas monitoring sketch using
 * BlynkSimpleEsp32 instead of the full Edgent framework.
 * WiFi credentials are hardcoded — no captive portal.
 *
 * Red/green LEDs indicate whether the reading is above or below
 * a configurable threshold.
 *
 * Blynk Virtual Pins:
 *   V1 — Analog gas sensor value (publish)
 *
 * Hardware: ESP32 + MQ-series analog gas sensor + red/green LEDs
 *
 * Pin Mapping:
 *   GPIO32 — Analog gas sensor input
 *   GPIO23 — Red LED (alert)
 *   GPIO22 — Green LED (normal)
 *
 * Copyright (c) 2026 Roboshop Technologies Sdn Bhd. All rights reserved.
 */

// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID "TMPL6clTUsFEW"
#define BLYNK_TEMPLATE_NAME "Gas Monitoring"
#define BLYNK_AUTH_TOKEN "d1jLp_1Kt1bXHMgezZIwnCX7Y9EGTqAp"

#include <BlynkSimpleEsp32.h>

#define wifi_username "insert your wifi username"
#define wifi_password "insert your wifi password"

//pinout declaratiorn
#define analogsensorpin 32 //change to where analog sensor is connected
#define led_red 23
#define led_green 22

#define treshold 500 //change the treshold value according to your sensor


void setup()
{
  Serial.begin(115200);
  delay(100);
  pinMode(led_red,OUTPUT);
  pinMode(led_green,OUTPUT);
  Blynk.begin(BLYNK_AUTH_TOKEN, wifi_username, wifi_password);
}

void loop() {
  Blynk.run();

  int analogvalue = analogRead(analogsensorpin);

  Blynk.virtualWrite(V1,analogvalue);

  //print value to Serial Monitor

  Serial.print("Gas Value : "); //change this to what you measure
  Serial.println(analogvalue);


  //led indicator process at the edge
  
  //chage the comparator on what are you measure
  if(analogvalue < treshold){
    digitalWrite(led_red,HIGH);
    digitalWrite(led_green,LOW);  
  }
  else
  {
    digitalWrite(led_red,LOW);
    digitalWrite(led_green,HIGH);
    
  }

  delay(1000);
}
