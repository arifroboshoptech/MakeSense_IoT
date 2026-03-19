/**
 *Version V1.0.1 
 *FarmSense IoT - ESP32 Soil Moisture Sensor Node
 *
 * Reads analog soil moisture sensor on GPIO35 and publishes
 * readings to the web dashboard via MQTT.
 *
 * WiFi: Managed by WiFiManager (tzapu). GPIO2 button triggers
 *       the config portal AP. AP shuts down once WiFi connects.
 *       Press GPIO2 again at any time to reopen the AP.
 *
 * MQTT Topics:
 *   Publish:   farm/sensors/{deviceId}/data     (soil moisture readings)
 *   Publish:   farm/sensors/{deviceId}/status   (heartbeat / device health)
 *   Subscribe: farm/sensors/{deviceId}/config   (remote configuration)
 *
 * Hardware:
 *   - ESP32 DevKit
 *   - Analog capacitive soil moisture sensor → GPIO35 (ADC1_CH7)
 *   - Push button → GPIO2 (press to open WiFi config portal)
 *   - On-board LED → GPIO2 (shared with button on most DevKit boards)
 *
 * Data format: JSON, compatible with FarmSense IoT web/mobile apps
 *
 * Copyright (c) 2026 Roboshop Technologies Sdn Bhd. All rights reserved.
 */

#include <WiFi.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =====================================================================
//  DEVICE CONFIGURATION — Edit these for each physical sensor node
// =====================================================================

const char* DEVICE_ID   = "farmsense_node_001";              // Unique device identifier  ==== UBAH NOMBOR 001-007
const char* DEVICE_NAME = "Soil Moisture Sensor 1"; // Friendly name shown in app === UBAH NOMBOR 1-7
const char* DEVICE_ZONE = "Line 1";                 // Zone assignment ===UBAH LINE 1-7
const char* DEVICE_TYPE = "soil_moisture";           // Sensor type (matches backend)

// =====================================================================
//  WIFI MANAGER CONFIGURATION
// =====================================================================

const char* AP_NAME     = "FarmSense-Soil-Node1";   // Captive portal SSID ===UBAH AP Name FarmSense-Soil-Node1-7
const char* AP_PASSWORD = "farmsense123";     // Captive portal password (min 8 chars)
const int   AP_TIMEOUT  = 180;                // Portal timeout in seconds

// =====================================================================
//  MQTT BROKER CONFIGURATION
// =====================================================================

const char* MQTT_BROKER   = "broker.hivemq.com";  // Broker hostname or IP
const int   MQTT_PORT     = 1883;                  // 1883 = TCP, 8883 = TLS
const char* MQTT_USER     = "";                    // Leave empty if no auth
const char* MQTT_PASSWORD = "";                    // Leave empty if no auth
const char* MQTT_PREFIX   = "farm/sensors";        // Topic prefix (matches backend)

// =====================================================================
//  HARDWARE PIN MAPPING
// =====================================================================

const int SOIL_PIN   = 35;   // GPIO35 (ADC1_CH7) — analog soil moisture sensor
const int CONFIG_PIN = 2;    // GPIO2  — push button to trigger WiFi config portal
const int LED_PIN    = 2;    // GPIO2  — on-board LED (shared with button)

// =====================================================================
//  SOIL SENSOR CALIBRATION
// =====================================================================
//  Measure your sensor's ADC output in dry air and submerged in water.
//  ESP32 ADC is 12-bit (0–4095).

int SOIL_DRY_VALUE = 3200;   // ADC value when sensor is in dry air
int SOIL_WET_VALUE = 1200;   // ADC value when sensor is in water

// Quality thresholds (percentage)
const int MOISTURE_CRITICAL = 20;   // Below = critical (needs water urgently)
const int MOISTURE_WARNING  = 40;   // Below = warning (getting dry)
const int MOISTURE_HIGH     = 85;   // Above = too wet

// Number of samples to average per reading (reduces noise)
const int SAMPLE_COUNT = 10;

// =====================================================================
//  TIMING
// =====================================================================

const unsigned long SENSOR_INTERVAL    = 10000;  // Read & publish every 10s
const unsigned long HEARTBEAT_INTERVAL = 30000;  // Heartbeat every 30s
const unsigned long MQTT_RETRY_DELAY   = 5000;   // Retry MQTT connection every 5s

// =====================================================================
//  MQTT TOPICS (built at runtime from DEVICE_ID)
// =====================================================================

String topicData;     // Publish:   .../data
String topicStatus;   // Publish:   .../status
String topicConfig;   // Subscribe: .../config

// =====================================================================
//  RUNTIME STATE
// =====================================================================

WiFiClient   espClient;
PubSubClient mqtt(espClient);
WiFiManager  wifiManager;

// Sensor data
int   soilPercent  = 0;
int   soilRaw      = 0;

// Timing
unsigned long lastSensorRead  = 0;
unsigned long lastHeartbeat   = 0;
unsigned long bootTime        = 0;
bool          mqttConnected   = false;

// Config button state tracking (edge detection: HIGH→LOW triggers portal)
bool lastConfigPinState = HIGH;

// =====================================================================
//  SETUP
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n========================================"));
  Serial.println(F("FarmSense IoT - Soil Moisture Sensor v1.0"));
  Serial.println(F("========================================"));
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.printf("Zone:      %s\n", DEVICE_ZONE);
  Serial.printf("Soil Pin:  GPIO%d\n", SOIL_PIN);
  Serial.printf("Config Pin: GPIO%d\n", CONFIG_PIN);

  // ---- Hardware init ----
  pinMode(SOIL_PIN, INPUT);
  pinMode(CONFIG_PIN, INPUT_PULLUP);  // Button with internal pull-up (press = LOW)
  delay(50);

  // ---- Build MQTT topics ----
  buildTopics();

  // ---- WiFiManager setup ----
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Check GPIO2: if button pressed (LOW) at boot, open config portal
  if (digitalRead(CONFIG_PIN) == LOW) {
    Serial.println(F("Config button pressed at boot — opening WiFi config portal"));
    if (!wifiManager.startConfigPortal(AP_NAME, AP_PASSWORD)) {
      Serial.println(F("WiFiManager: portal timed out, restarting..."));
      delay(1000);
      ESP.restart();
    }
  } else {
    // Normal boot: auto-connect using saved credentials, portal on first boot
    Serial.println(F("Connecting to WiFi..."));
    if (!wifiManager.autoConnect(AP_NAME, AP_PASSWORD)) {
      Serial.println(F("WiFiManager: portal timed out, restarting..."));
      delay(1000);
      ESP.restart();
    }
  }

  // WiFi connected — shut down the AP
  Serial.println(F("WiFi connected!"));
  Serial.printf("IP:   %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());

  shutdownAP();

  // Track initial button state for edge detection in loop
  lastConfigPinState = digitalRead(CONFIG_PIN);

  // ---- MQTT setup ----
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);

  connectMQTT();

  bootTime = millis();

  // Publish initial status
  publishHeartbeat();

  Serial.println(F("\nSetup complete. Reading soil moisture...\n"));
}

// =====================================================================
//  MAIN LOOP
// =====================================================================

void loop() {
  unsigned long now = millis();

  // ---- WiFi watchdog ----
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi lost! Restarting..."));
    delay(1000);
    ESP.restart();
  }

  // ---- MQTT reconnect ----
  if (!mqtt.connected()) {
    mqttConnected = false;
    connectMQTT();
  }

  mqtt.loop();

  // ---- Read sensor & publish ----
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readSoilMoisture();
    publishSensorData();
    lastSensorRead = now;
  }

  // ---- Heartbeat ----
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = now;
  }

  // ---- Config button: GPIO2 pressed → open WiFi portal ----
  bool currentConfigPin = digitalRead(CONFIG_PIN);
  if (currentConfigPin == LOW && lastConfigPinState == HIGH) {
    // Edge detected: button just pressed
    delay(50);  // Debounce
    if (digitalRead(CONFIG_PIN) == LOW) {
      Serial.println(F("Config button pressed — opening WiFi config portal"));

      // Open the config portal (AP becomes visible)
      wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
      wifiManager.startConfigPortal(AP_NAME, AP_PASSWORD);

      // Portal closed (WiFi saved or timed out) — shut down AP
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("WiFi connected — shutting down AP"));
        shutdownAP();
      } else {
        Serial.println(F("WiFi not connected after portal, restarting..."));
        delay(1000);
        ESP.restart();
      }
    }
  }
  lastConfigPinState = currentConfigPin;

  delay(50);
}

// =====================================================================
//  MQTT TOPIC BUILDER
// =====================================================================

void buildTopics() {
  topicData   = String(MQTT_PREFIX) + "/" + DEVICE_ID + "/data";
  topicStatus = String(MQTT_PREFIX) + "/" + DEVICE_ID + "/status";
  topicConfig = String(MQTT_PREFIX) + "/" + DEVICE_ID + "/config";

  Serial.println(F("\nMQTT Topics:"));
  Serial.printf("  Publish data:   %s\n", topicData.c_str());
  Serial.printf("  Publish status: %s\n", topicStatus.c_str());
  Serial.printf("  Subscribe cfg:  %s\n", topicConfig.c_str());
}

// =====================================================================
//  MQTT CONNECTION
// =====================================================================

void connectMQTT() {
  if (mqttConnected) return;

  Serial.printf("Connecting to MQTT %s:%d\n", MQTT_BROKER, MQTT_PORT);

  String clientId = String("esp32_") + DEVICE_ID + "_" + String(random(0xFFFF), HEX);

  for (int attempt = 0; attempt < 5; attempt++) {
    Serial.printf("  Attempt %d/5...", attempt + 1);

    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    } else {
      ok = mqtt.connect(clientId.c_str());
    }

    if (ok) {
      mqttConnected = true;
      Serial.println(F(" connected!"));

      // Subscribe to config topic for remote updates
      mqtt.subscribe(topicConfig.c_str());
      Serial.printf("  Subscribed: %s\n", topicConfig.c_str());
      return;
    }

    Serial.printf(" failed (rc=%d)\n", mqtt.state());
    delay(MQTT_RETRY_DELAY);
  }

  Serial.println(F("MQTT connection failed after 5 attempts"));
}

// =====================================================================
//  MQTT CALLBACK — Handle config messages
// =====================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

  if (topicStr == topicConfig) {
    handleConfig(message);
  }
}

// =====================================================================
//  CONFIG HANDLER — Remote config updates via MQTT
// =====================================================================

void handleConfig(const String& message) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, message);

  if (err) {
    Serial.printf("Config parse error: %s\n", err.c_str());
    return;
  }

  // Update calibration values
  if (doc.containsKey("soilDryValue")) {
    SOIL_DRY_VALUE = doc["soilDryValue"].as<int>();
    Serial.printf("Soil dry calibration = %d\n", SOIL_DRY_VALUE);
  }

  if (doc.containsKey("soilWetValue")) {
    SOIL_WET_VALUE = doc["soilWetValue"].as<int>();
    Serial.printf("Soil wet calibration = %d\n", SOIL_WET_VALUE);
  }

  // Handle reboot command
  if (doc.containsKey("reboot") && doc["reboot"].as<bool>()) {
    Serial.println(F("Reboot requested via MQTT"));
    delay(500);
    ESP.restart();
  }
}

// =====================================================================
//  SOIL MOISTURE READING — Multi-sample averaged
// =====================================================================

void readSoilMoisture() {
  long total = 0;

  // Take multiple samples and average to reduce noise
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    total += analogRead(SOIL_PIN);
    delay(10);
  }

  soilRaw = total / SAMPLE_COUNT;
  soilPercent = map(soilRaw, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  Serial.printf("Soil Moisture: %d%% (raw: %d)\n", soilPercent, soilRaw);
}

// =====================================================================
//  PUBLISH: Sensor Data — JSON for web dashboard
// =====================================================================

void publishSensorData() {
  if (!mqttConnected) return;

  StaticJsonDocument<384> doc;
  doc["deviceId"]  = DEVICE_ID;
  doc["name"]      = DEVICE_NAME;
  doc["zone"]      = DEVICE_ZONE;
  doc["type"]      = DEVICE_TYPE;
  doc["timestamp"] = millis();

  // Sensor readings object (matches backend SensorReading model)
  JsonObject sensors = doc.createNestedObject("sensors");

  JsonObject soil = sensors.createNestedObject("soil_moisture");
  soil["value"]   = soilPercent;
  soil["unit"]    = "percent";
  soil["raw"]     = soilRaw;
  soil["quality"] = getQuality(soilPercent);

  String json;
  serializeJson(doc, json);

  if (mqtt.publish(topicData.c_str(), json.c_str())) {
    Serial.println(F("Sensor data published"));
  } else {
    Serial.println(F("Failed to publish sensor data"));
  }
}

// =====================================================================
//  PUBLISH: Heartbeat — Device health
// =====================================================================

void publishHeartbeat() {
  if (!mqttConnected) return;

  StaticJsonDocument<256> doc;
  doc["deviceId"]  = DEVICE_ID;
  doc["status"]    = "online";
  doc["uptime"]    = (millis() - bootTime) / 1000;
  doc["rssi"]      = WiFi.RSSI();
  doc["freeHeap"]  = ESP.getFreeHeap();
  doc["soilRaw"]   = soilRaw;
  doc["soilPct"]   = soilPercent;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);
  mqtt.publish(topicStatus.c_str(), json.c_str());
}

// =====================================================================
//  QUALITY ASSESSMENT
// =====================================================================

const char* getQuality(int moisture) {
  if (moisture < MOISTURE_CRITICAL) return "critical";
  if (moisture < MOISTURE_WARNING)  return "warning";
  if (moisture > MOISTURE_HIGH)     return "warning";
  return "good";
}

// =====================================================================
//  SHUT DOWN AP — Disable soft-AP and switch to STA-only mode
// =====================================================================

void shutdownAP() {
  WiFi.softAPdisconnect(true);
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);
  Serial.println(F("AP disabled, STA-only mode"));
}

// =====================================================================
//  WIFI MANAGER CALLBACKS
// =====================================================================

void configModeCallback(WiFiManager* myWiFiManager) {
  Serial.println(F("WiFiManager: Entered config portal"));
  Serial.printf("AP IP:   %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP SSID: %s\n", myWiFiManager->getConfigPortalSSID().c_str());
}

void saveConfigCallback() {
  Serial.println(F("WiFiManager: Config saved"));
}
