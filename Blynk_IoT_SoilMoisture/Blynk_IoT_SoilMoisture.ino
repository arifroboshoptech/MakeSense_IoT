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
