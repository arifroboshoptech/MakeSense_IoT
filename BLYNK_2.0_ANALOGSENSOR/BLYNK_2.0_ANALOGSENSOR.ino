// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID "TMPL6KIggV9ie"
#define BLYNK_TEMPLATE_NAME "Gas Monitoring"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"
#define BLYNK_PRINT Serial
#define APP_DEBUG
#include "BlynkEdgent.h"

//pinout declaratiorn
#define analogsensorpin 36 //change to where analog sensor is connected
#define led_red 23
#define led_green 22

#define treshold 500 //change the treshold value according to your sensor


void setup()
{
  Serial.begin(115200);
  delay(100);
  pinMode(led_red,OUTPUT);
  pinMode(led_green,OUTPUT);
  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();

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
