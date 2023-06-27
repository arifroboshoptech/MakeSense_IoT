// Template ID, Device Name and Auth Token are provided by the Blynk.Cloud
// See the Device Info tab, or Template settings
#define BLYNK_TEMPLATE_ID "TMPLlh8o1jPo" //tukar tempalte id
#define BLYNK_DEVICE_NAME "Getting started" //tukar device name
#define BLYNK_AUTH_TOKEN "57xQ3I8tMFX5tsqzAuleqJsI0xU2wfaA" //tukar token


// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

char auth[] = BLYNK_AUTH_TOKEN;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "AndroidAP3DEC"; //wifi username
char pass[] = "electroniclinic"; //wifi password

BlynkTimer timer;



int soil_pin=26; //tukar kepada pinout analog sensor anda
int led = 23; //tukar kepada pinout led anda

BLYNK_WRITE(V0)
{
  int pinValue=param.asInt();
  digitalWrite(led,pinValue);
  }
void setup()
{
    //pinMode(pot,INPUT);
  pinMode(25,OUTPUT);
  // Debug console
  Serial.begin(115200);

  Blynk.begin(auth, ssid, pass);

}

void loop()
{
  Blynk.run();
   int soil_value=analogRead(soil_pin);
  Blynk.virtualWrite(V1,soil_value);
}
