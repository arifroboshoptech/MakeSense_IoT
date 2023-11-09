#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>

#define DHTPIN 4
#define LED 17
#define RELAY 16 // Relay pin
#define BUZZER 13 // Buzzer pin
#define DHTTYPE DHT11

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

const char *ssid = "OPPO Reno10 Pro 5G";
const char *password = "aqil1234";
const char *mqttServer = "broker.hivemq.com";
const char *topic = "Tempdata12";
const char *relayTopic = "RelayHome1"; // New MQTT topic for relay control
String relayTopic2 = "lights";

WiFiClient espClient;
PubSubClient client(espClient);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("espClient")) {
      Serial.println("MQTT connected");
      client.subscribe(relayTopic2);
      client.subscribe(relayTopic); // Subscribe to the relay topic
      Serial.println("Topic Subscribed");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  String data = "";
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];
  }
  Serial.println("Message received: " + data);
  if (data == "ON") {
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(LED, LOW);
  }
  
  // Check if the received message is for the relay
  if (String(topic) == relayTopic) {
    if (data == "ON") {
      digitalWrite(RELAY, HIGH); // Turn on the relay
      tone(BUZZER, 1000, 2000); // Turn on the buzzer for 2 seconds
    } else {
      digitalWrite(RELAY, LOW); // Turn off the relay
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT); // Set relay pin as output
  pinMode(BUZZER, OUTPUT); // Set buzzer pin as output
  digitalWrite(LED, LOW);
  digitalWrite(RELAY, LOW); // Initialize relay state to off
  digitalWrite(BUZZER, LOW); // Initialize buzzer state to off

  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  delay(2000);
  display.clearDisplay();
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  static unsigned long previousMillis = 0;
  const long interval = 5000; // Update every 5 seconds

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    sensors_event_t event;
    dht.temperature().getEvent(&event);

    float temp, hum;

    if (isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    } else {
      Serial.print(F("Temperature: "));
      temp = event.temperature;
      Serial.print(temp);
      Serial.println(F("°C"));
    }

    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    } else {
      Serial.print(F("Humidity: "));
      hum = event.relative_humidity;
      Serial.print(hum);
      Serial.println(F("%"));
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    if (WiFi.status() == WL_CONNECTED) {
      display.println("Wi-Fi Connected");
      display.println("IP Address:");
      digitalWrite(BUZZER, HIGH);
      delay(2000);
      digitalWrite(BUZZER, LOW);
      display.println(WiFi.localIP());
    } else {
      display.println("Wi-Fi Disconnected");
    }

    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("Temp: ");
    display.print(temp);
    display.println("°C");
    display.print("Humidity: ");
    display.print(hum);
    display.println("%");
    display.display();

    String msgStr = String(temp) + "," + String(hum);
    char msg[msgStr.length() + 1];
    msgStr.toCharArray(msg, sizeof(msg));
    client.publish(topic, msg);
    delay(50);
  }
}
