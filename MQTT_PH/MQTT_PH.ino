#include <Wire.h>
#include <WiFi.h>
#include <MQTT.h>
#include <WiFiManager.h>
#include <LiquidCrystal_I2C.h>
#include "DFRobot_ESP_PH.h"
#include <EEPROM.h>

#define WIFI_SSID             "PSDC_Kit1" //Access point name max 13 character
#define WIFI_PASSWORD         "PSDC1234"
#define MQTT_HOST             "broker.hivemq.com"
#define MQTT_PREFIX_TOPIC     "PSDC/phkit01/mqtt" //can chage topic prefix
#define MQTT_PUBLISH_TOPIC    "/data"
#define MQTT_SUBSCRIBE_TOPIC  "/control"

#define TRIGGER_PIN 23
#define Buzzer 13

int timeout = 120;

LiquidCrystal_I2C lcd(0x27, 16, 2);



DFRobot_ESP_PH ph;
#define ESPADC 4096.0   //the esp Analog Digital Convertion value
#define ESPVOLTAGE 3300 //the esp voltage supply value
#define PH_PIN 35		//the esp gpio data pin number
float voltage, phValue, temperature = 25;




WiFiClient net;
MQTTClient mqtt(1024);

String wifi_status;

unsigned long lastMillis = 0;

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connect to WiFi ");
  lcd.setCursor(0, 1);
  lcd.print("AP:"+String(WIFI_SSID));

  WiFi.mode(WIFI_STA);
  //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  WiFiManager wm; 
  wm.setConfigPortalTimeout(timeout); 

  if (!wm.startConfigPortal(WIFI_SSID)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected");
    wifi_status = "ON ";
    lcd.clear();
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Incoming Status: " + payload);
  Serial.println();
}

void connectToMqttBroker(){
  Serial.print("Connecting to '" + String(MQTT_HOST) + "' ...");
  
  mqtt.begin(MQTT_HOST, net);
  mqtt.onMessage(messageReceived);
  

  String uniqueString = String(WIFI_SSID) + "-" + String(random(1, 98)) + String(random(99, 999));
  char uniqueClientID[uniqueString.length() + 1];
  
  uniqueString.toCharArray(uniqueClientID, uniqueString.length() + 1);
  
  while (!mqtt.connect(uniqueClientID)) {
    Serial.print(".");
    delay(500);
  }

  Serial.println(" connected!");

  Serial.println("Subscribe to: " + String(MQTT_PREFIX_TOPIC) + String(MQTT_SUBSCRIBE_TOPIC));
  
  mqtt.subscribe(String(MQTT_PREFIX_TOPIC) + String(MQTT_SUBSCRIBE_TOPIC));

}


void setup() {
  
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(Buzzer,OUTPUT);
  lcd.begin();
  lcd.backlight();
  EEPROM.begin(32);
  delay(2000);
  lcd.setCursor(0,0);
  lcd.print(" IOT PH MONITOR ");

  Serial.println("\nESP32 MQTT");
  //connectToWiFi();
  //connectToMqttBroker();

  Serial.println();

}

void loop() { 
 
  mqtt.loop();
  delay(10);  // <- fixes some issues with WiFi stability


  //Online Mode Triggered
  if (digitalRead(TRIGGER_PIN) == HIGH){
    Serial.println("Button Triggered");
      digitalWrite(Buzzer,HIGH);
      delay(500);
      digitalWrite(Buzzer,LOW);
      delay(500);
      digitalWrite(Buzzer,HIGH);
      delay(500);
      digitalWrite(Buzzer,LOW);

    connectToWiFi();
    connectToMqttBroker();

    if (!mqtt.connected()) {
      connectToMqttBroker();
    }
     
  }

  if (WiFi.status() != WL_CONNECTED){
      wifi_status = "OFF";
    }  


    //Display LCD
    lcd.setCursor(0,0);
    lcd.print(" IOT PH MONITOR ");
    lcd.setCursor(0,1);  
    lcd.print("PH:" + String(phValue)+" WiFi:" + wifi_status);

     
    


    //send via mqtt every 10ms 1s=1000ms

     static unsigned long timepoint = millis();
	if (millis() - timepoint > 1000U) //time interval: 1s
	{
		timepoint = millis();
		//voltage = rawPinValue / esp32ADC * esp32Vin
		voltage = analogRead(PH_PIN) / ESPADC * ESPVOLTAGE; // read the voltage
		Serial.print("voltage:");
		Serial.println(voltage, 4);
		
		//temperature = readTemperature();  // read your temperature sensor to execute temperature compensation
		Serial.print("temperature:");
		Serial.print(temperature, 1);
		Serial.println("^C");

		phValue = ph.readPH(voltage, temperature); // convert voltage to pH with temperature compensation
		Serial.print("pH:");
		Serial.println(phValue, 4);
	}
	ph.calibration(voltage, temperature); // calibration process by Serail CMD



  //send to mqtt broker every 10s

  if (millis() - lastMillis > 10000) {
    lastMillis = millis();

    String dataInJson = "{";
    dataInJson += "\"phvalue\":" + String(phValue) ;
    dataInJson += "}";

    Serial.println("Data to Publish: " + dataInJson);
    Serial.println("Length of Data: " + String(dataInJson.length()));
    Serial.println("Publish to: " + String(MQTT_PREFIX_TOPIC) + String(MQTT_PUBLISH_TOPIC));
    
    mqtt.publish(String(MQTT_PREFIX_TOPIC) + String(MQTT_PUBLISH_TOPIC), dataInJson);
 
   }

  Serial.println();

  delay(2000);   

  

}
