// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID "TMPL6clTUsFEW"
#define BLYNK_TEMPLATE_NAME "Gas Monitoring"
#define BLYNK_AUTH_TOKEN "d1jLp_1Kt1bXHMgezZIwnCX7Y9EGTqAp"

#include <BlynkSimpleEsp32.h>

#define wifi_username "your wifi username"
#define wifi_password "your wifi password"

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
