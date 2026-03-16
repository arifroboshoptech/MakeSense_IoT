/**
 *Version 1.0.1 
 *FarmSense IoT - ESP8266 4-Channel Relay Controller
 *
 * Controls 4 relay channels via MQTT for smart farm actuators.
 * Each relay has its own MQTT command/state topics mapped to app widgets.
 *
 * WiFi: Managed by WiFiManager (captive portal on first boot)
 *       AP hidden once STA connection is established.
 *
 * MQTT Topics per relay (N = 1..4):
 *   Subscribe: farm/actuators/{deviceId}_chN/command   (receive ON/OFF/TOGGLE)
 *   Publish:   farm/actuators/{deviceId}_chN/state     (report relay state)
 *   Publish:   farm/actuators/{deviceId}_chN/status    (heartbeat)
 *   Publish:   farm/actuators/{deviceId}_chN/response  (command ack)
 *
 * Hardware: ESP8266 + 4-Channel Relay Module (active LOW)
 *
 * Data format: JSON, compatible with FarmSense IoT web/mobile apps
 *
 * Copyright (c) 2026 Roboshop Technologies Sdn Bhd. All rights reserved.
 */

#include <ESP8266WiFi.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// =====================================================================
//  DEVICE CONFIGURATION — Edit these for each physical device
// =====================================================================

const char* DEVICE_ID   = "pumpcontroller_001";          // Unique device identifier
const char* DEVICE_NAME = "Pump Controller"; // Friendly name shown in app
const char* DEVICE_ZONE = "Pondok";           // Zone assignment

// Per-channel configuration: type and name shown on app widget
// Valid types: "pump", "fan", "light", "valve", "heater", "generic"
struct ChannelConfig {
  const char* name;     // Widget display name
  const char* type;     // Actuator category (matches app filter)
  int         power;    // Estimated power draw in watts when ON
};

const ChannelConfig CHANNELS[4] = {
  { "Water Pump",       "pump",  45  },   // CH1
  { "Exhaust Fan",      "fan",   120 },   // CH2
  { "Grow Light",       "light", 200 },   // CH3
  { "Solenoid Valve",   "valve", 15  },   // CH4
};

// =====================================================================
//  MQTT BROKER CONFIGURATION
// =====================================================================

const char* MQTT_BROKER   = "broker.hivemq.com";  // Broker hostname or IP
const int   MQTT_PORT     = 1883;                  // 1883 = TCP, 8883 = TLS
const char* MQTT_USER     = "";                    // Leave empty if no auth
const char* MQTT_PASSWORD = "";                    // Leave empty if no auth
const char* MQTT_PREFIX   = "farm/actuators";      // Topic prefix (matches backend)

// =====================================================================
//  HARDWARE PIN MAPPING — ESP8266 GPIOs to relay inputs
// =====================================================================
//  Relay module is ACTIVE HIGH (GPIO HIGH = relay ON, LOW = relay OFF)

const int RELAY_PINS[4] = {
  16,  // GPIO16  — Relay CH1
  14,  // GPIO14  — Relay CH2
  12,  // GPIO12 — Relay CH3
  13,  // GPIO13 — Relay CH4
};

const int LED_PIN    = LED_BUILTIN;  // On-board LED for status indication
const int CONFIG_PIN = 15;          // GPIO15 (D8) — pull LOW to open WiFi config portal

// =====================================================================
//  WIFI MANAGER CONFIGURATION
// =====================================================================

const char* AP_NAME     = "FarmSense-Relay";  // Captive portal SSID
const char* AP_PASSWORD = "farmsense123";     // Captive portal password (min 8 chars)
const int   AP_TIMEOUT  = 180;                // Portal timeout in seconds

// =====================================================================
//  SAFETY & TIMING
// =====================================================================

unsigned long MAX_RUN_DURATION[4]   = { 300000, 300000, 300000, 300000 };  // 5 min per ch
unsigned long COOLDOWN_PERIOD[4]    = { 60000,  60000,  60000,  60000  };  // 1 min per ch

const unsigned long STATE_INTERVAL     = 5000;   // Publish state every 5s
const unsigned long HEARTBEAT_INTERVAL = 30000;  // Heartbeat every 30s
const unsigned long MQTT_RETRY_DELAY   = 5000;   // Retry MQTT connection every 5s

const int NUM_CHANNELS = 4;

// =====================================================================
//  MQTT TOPICS (built at runtime from DEVICE_ID)
// =====================================================================

String topicCommand[4];   // Subscribe: .../command
String topicState[4];     // Publish:   .../state
String topicStatus[4];    // Publish:   .../status
String topicResponse[4];  // Publish:   .../response
String topicConfig[4];    // Subscribe: .../config

// =====================================================================
//  RUNTIME STATE
// =====================================================================

WiFiClient   espClient;
PubSubClient mqtt(espClient);
WiFiManager  wifiManager;

// Per-channel state
bool          relayState[4]    = { false, false, false, false };
unsigned long stateStartTime[4] = { 0, 0, 0, 0 };
unsigned long lastStopTime[4]  = { 0, 0, 0, 0 };
unsigned long totalRunTime[4]  = { 0, 0, 0, 0 };

// Global timing
unsigned long lastStatePublish   = 0;
unsigned long lastHeartbeat      = 0;
unsigned long bootTime           = 0;
bool          mqttConnected      = false;

// Config button state tracking (edge detection: HIGH→LOW triggers portal)
bool lastConfigPinState = HIGH;

// =====================================================================
//  SETUP
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n========================================"));
  Serial.println(F("FarmSense IoT - ESP8266 4CH Relay v1.0"));
  Serial.println(F("========================================"));
  Serial.print(F("Device ID: ")); Serial.println(DEVICE_ID);
  Serial.print(F("Zone:      ")); Serial.println(DEVICE_ZONE);

  // ---- Hardware init: all relays OFF ----
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);   // LOW = relay OFF (active high)
  }
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off (active low on most ESP8266 boards)

  // ---- Config button: GPIO15 to GND = open WiFi config portal ----
  pinMode(CONFIG_PIN, INPUT);
  delay(50);  // Debounce settle time

  // ---- Build MQTT topics ----
  buildTopics();

  // ---- WiFiManager setup ----
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Check GPIO15: if LOW (connected to GND), open config portal
  if (digitalRead(CONFIG_PIN) == LOW) {
    Serial.println(F("GPIO15 LOW — opening WiFi config portal"));
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

  // WiFi connected — shut down the AP completely
  Serial.println(F("WiFi connected!"));
  Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
  Serial.print(F("RSSI: ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));

  shutdownAP();

  // Track initial pin state for edge detection in loop
  lastConfigPinState = digitalRead(CONFIG_PIN);

  // ---- MQTT setup ----
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);

  connectMQTT();

  bootTime = millis();

  // Publish initial state for all channels
  for (int i = 0; i < NUM_CHANNELS; i++) {
    publishState(i, "Device started");
    publishHeartbeat(i);
  }

  Serial.println(F("\nSetup complete. Waiting for MQTT commands...\n"));
}

// =====================================================================
//  MAIN LOOP
// =====================================================================

void loop() {
  unsigned long now = millis();

  // ---- WiFi watchdog ----
  if (WiFi.status() != WL_CONNECTED) {
    // Safety: turn all relays OFF on WiFi loss
    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (relayState[i]) {
        setRelay(i, false, "WiFi disconnected - safety shutoff");
      }
    }
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

  // Refresh timestamp after mqtt.loop() — commands processed inside may have
  // set stateStartTime to a newer millis() value than the `now` captured above.
  // Without this, unsigned subtraction underflows and triggers false safety shutoff.
  now = millis();

  // ---- Safety: max run duration per channel ----
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (relayState[i] && (now - stateStartTime[i] >= MAX_RUN_DURATION[i])) {
      setRelay(i, false, "Max duration reached - safety shutoff");
      Serial.printf("SAFETY: CH%d OFF - max duration\n", i + 1);
    }
  }

  // ---- Periodic state publish ----
  if (now - lastStatePublish >= STATE_INTERVAL) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      publishState(i, NULL);
    }
    lastStatePublish = now;
  }

  // ---- Periodic heartbeat ----
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      publishHeartbeat(i);
    }
    lastHeartbeat = now;
  }

  // ---- Config button: GPIO15 goes LOW → open WiFi portal ----
  bool currentConfigPin = digitalRead(CONFIG_PIN);
  if (currentConfigPin == LOW && lastConfigPinState == HIGH) {
    // Edge detected: GPIO15 just connected to GND
    Serial.println(F("GPIO15 LOW detected — opening WiFi config portal"));

    // Safety: turn all relays OFF before entering portal
    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (relayState[i]) {
        setRelay(i, false, "Config portal - safety shutoff");
      }
    }

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
  lastConfigPinState = currentConfigPin;

  // ---- LED: blink if any relay is ON ----
  bool anyOn = false;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (relayState[i]) { anyOn = true; break; }
  }
  digitalWrite(LED_PIN, anyOn ? LOW : HIGH);  // Active low LED

  delay(50);
}

// =====================================================================
//  MQTT TOPIC BUILDER
// =====================================================================

void buildTopics() {
  Serial.println(F("\nMQTT Topics:"));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    String chId = String(DEVICE_ID) + "_ch" + String(i + 1);
    topicCommand[i]  = String(MQTT_PREFIX) + "/" + chId + "/command";
    topicState[i]    = String(MQTT_PREFIX) + "/" + chId + "/state";
    topicStatus[i]   = String(MQTT_PREFIX) + "/" + chId + "/status";
    topicResponse[i] = String(MQTT_PREFIX) + "/" + chId + "/response";
    topicConfig[i]   = String(MQTT_PREFIX) + "/" + chId + "/config";

    Serial.printf("  CH%d [%s]: cmd=%s\n", i + 1, CHANNELS[i].name, topicCommand[i].c_str());
  }
}

// =====================================================================
//  MQTT CONNECTION
// =====================================================================

void connectMQTT() {
  if (mqttConnected) return;

  Serial.printf("Connecting to MQTT %s:%d\n", MQTT_BROKER, MQTT_PORT);

  String clientId = String("esp8266_") + DEVICE_ID + "_" + String(random(0xFFFF), HEX);

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

      // Subscribe to command & config topics for all channels
      for (int i = 0; i < NUM_CHANNELS; i++) {
        mqtt.subscribe(topicCommand[i].c_str());
        mqtt.subscribe(topicConfig[i].c_str());
        Serial.printf("  Subscribed: %s\n", topicCommand[i].c_str());
      }
      return;
    }

    Serial.printf(" failed (rc=%d)\n", mqtt.state());
    delay(MQTT_RETRY_DELAY);
  }

  Serial.println(F("MQTT connection failed after 5 attempts"));
}

// =====================================================================
//  MQTT CALLBACK — Route messages to correct channel handler
// =====================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

  // Determine which channel this message is for
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (topicStr == topicCommand[i]) {
      handleCommand(i, message);
      return;
    }
    if (topicStr == topicConfig[i]) {
      handleConfig(i, message);
      return;
    }
  }

  Serial.println(F("Unknown topic, ignoring"));
}

// =====================================================================
//  COMMAND HANDLER — Process ON/OFF/TOGGLE per channel
// =====================================================================

void handleCommand(int ch, const String& message) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, message);

  if (err) {
    Serial.printf("CH%d command parse error: %s\n", ch + 1, err.c_str());
    return;
  }

  const char* action = doc["action"];
  const char* source = doc["source"] | "unknown";
  const char* user   = doc["user"]   | "system";

  if (!action) {
    Serial.printf("CH%d: no action field\n", ch + 1);
    publishResponse(ch, false, "Missing action field");
    return;
  }

  Serial.printf("CH%d: action=%s source=%s user=%s\n", ch + 1, action, source, user);

  // Normalize action to uppercase for comparison
  String act(action);
  act.toUpperCase();

  // Cooldown only applies to non-manual sources (schedule, automation)
  // Manual commands from the app should always be honoured immediately
  bool isManual = (strcmp(source, "manual") == 0);

  if (act == "ON" || act == "START") {
    // Check cooldown (skip for manual commands)
    if (!isManual) {
      unsigned long now = millis();
      if (lastStopTime[ch] > 0 && (now - lastStopTime[ch] < COOLDOWN_PERIOD[ch])) {
        unsigned long remaining = (COOLDOWN_PERIOD[ch] - (now - lastStopTime[ch])) / 1000;
        char msg[48];
        snprintf(msg, sizeof(msg), "Cooldown active, %lu seconds left", remaining);
        Serial.printf("CH%d: cooldown blocked ON (%lu s left)\n", ch + 1, remaining);
        publishResponse(ch, false, msg);
        return;
      }
    }
    setRelay(ch, true, "Command received");
    publishResponse(ch, true, "Relay turned ON");
  }
  else if (act == "OFF" || act == "STOP") {
    setRelay(ch, false, "Command received");
    publishResponse(ch, true, "Relay turned OFF");
  }
  else if (act == "TOGGLE") {
    if (relayState[ch]) {
      setRelay(ch, false, "Toggle command");
      publishResponse(ch, true, "Toggled OFF");
    } else {
      if (!isManual) {
        unsigned long now = millis();
        if (lastStopTime[ch] > 0 && (now - lastStopTime[ch] < COOLDOWN_PERIOD[ch])) {
          Serial.printf("CH%d: cooldown blocked TOGGLE ON\n", ch + 1);
          publishResponse(ch, false, "Cooldown active");
          return;
        }
      }
      setRelay(ch, true, "Toggle command");
      publishResponse(ch, true, "Toggled ON");
    }
  }
  else if (act == "STATUS") {
    publishState(ch, "Status requested");
    publishResponse(ch, true, "Status published");
  }
  else {
    Serial.printf("CH%d: unknown action '%s'\n", ch + 1, action);
    publishResponse(ch, false, "Unknown action");
  }
}

// =====================================================================
//  CONFIG HANDLER — Remote config updates per channel
// =====================================================================

void handleConfig(int ch, const String& message) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, message);

  if (err) {
    Serial.printf("CH%d config parse error: %s\n", ch + 1, err.c_str());
    return;
  }

  bool changed = false;

  if (doc.containsKey("maxRunDuration")) {
    MAX_RUN_DURATION[ch] = doc["maxRunDuration"].as<unsigned long>();
    Serial.printf("CH%d maxRunDuration = %lu ms\n", ch + 1, MAX_RUN_DURATION[ch]);
    changed = true;
  }

  if (doc.containsKey("cooldownPeriod")) {
    COOLDOWN_PERIOD[ch] = doc["cooldownPeriod"].as<unsigned long>();
    Serial.printf("CH%d cooldownPeriod = %lu ms\n", ch + 1, COOLDOWN_PERIOD[ch]);
    changed = true;
  }

  if (doc.containsKey("reboot") && doc["reboot"].as<bool>()) {
    Serial.println(F("Reboot requested via MQTT"));
    for (int i = 0; i < NUM_CHANNELS; i++) {
      setRelay(i, false, "Reboot requested");
    }
    delay(500);
    ESP.restart();
  }

  if (changed) {
    Serial.printf("CH%d config updated\n", ch + 1);
  }
}

// =====================================================================
//  RELAY CONTROL — Set physical relay and track state
// =====================================================================

void setRelay(int ch, bool state, const char* reason) {
  unsigned long now = millis();

  if (state && !relayState[ch]) {
    // Turning ON
    stateStartTime[ch] = now;
    Serial.printf("CH%d ON [%s] ~%dW\n", ch + 1, CHANNELS[ch].name, CHANNELS[ch].power);
  }
  else if (!state && relayState[ch]) {
    // Turning OFF
    totalRunTime[ch] += (now - stateStartTime[ch]);
    lastStopTime[ch] = now;
    Serial.printf("CH%d OFF [%s] ran %lu s\n", ch + 1, CHANNELS[ch].name,
                  (now - stateStartTime[ch]) / 1000);
  }

  relayState[ch] = state;
  digitalWrite(RELAY_PINS[ch], state ? HIGH : LOW);   // Active HIGH relay

  // Immediately publish new state
  publishState(ch, reason);
}

// =====================================================================
//  PUBLISH: State — Widget-compatible JSON for each channel
// =====================================================================

void publishState(int ch, const char* reason) {
  if (!mqttConnected) return;

  unsigned long now = millis();
  String chDeviceId = String(DEVICE_ID) + "_ch" + String(ch + 1);

  StaticJsonDocument<384> doc;
  doc["deviceId"]  = chDeviceId;
  doc["name"]      = CHANNELS[ch].name;
  doc["zone"]      = DEVICE_ZONE;
  doc["type"]      = CHANNELS[ch].type;
  doc["state"]     = relayState[ch];
  doc["isOn"]      = relayState[ch];
  doc["power"]     = relayState[ch] ? CHANNELS[ch].power : 0;
  doc["timestamp"] = now;

  if (relayState[ch]) {
    unsigned long running = now - stateStartTime[ch];
    doc["runningFor"]    = running;
    doc["maxDuration"]   = MAX_RUN_DURATION[ch];
    doc["remainingTime"] = MAX_RUN_DURATION[ch] - running;
  } else {
    doc["runningFor"] = 0;
    if (lastStopTime[ch] > 0 && (now - lastStopTime[ch] < COOLDOWN_PERIOD[ch])) {
      doc["cooldownRemaining"] = COOLDOWN_PERIOD[ch] - (now - lastStopTime[ch]);
    }
  }

  doc["totalRunTime"] = totalRunTime[ch] + (relayState[ch] ? (now - stateStartTime[ch]) : 0);

  if (reason) {
    doc["reason"] = reason;
  }

  String json;
  serializeJson(doc, json);
  mqtt.publish(topicState[ch].c_str(), json.c_str());
}

// =====================================================================
//  PUBLISH: Heartbeat — Device health per channel
// =====================================================================

void publishHeartbeat(int ch) {
  if (!mqttConnected) return;

  unsigned long now = millis();
  String chDeviceId = String(DEVICE_ID) + "_ch" + String(ch + 1);

  StaticJsonDocument<256> doc;
  doc["deviceId"]      = chDeviceId;
  doc["status"]        = "online";
  doc["actuatorState"] = relayState[ch];
  doc["uptime"]        = (now - bootTime) / 1000;
  doc["rssi"]          = WiFi.RSSI();
  doc["freeHeap"]      = ESP.getFreeHeap();
  doc["timestamp"]     = now;

  String json;
  serializeJson(doc, json);
  mqtt.publish(topicStatus[ch].c_str(), json.c_str());
}

// =====================================================================
//  PUBLISH: Command response — Ack/nack for app
// =====================================================================

void publishResponse(int ch, bool success, const char* message) {
  if (!mqttConnected) return;

  StaticJsonDocument<128> doc;
  doc["success"]   = success;
  doc["message"]   = message;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);
  mqtt.publish(topicResponse[ch].c_str(), json.c_str());
}

// =====================================================================
//  SHUT DOWN AP — Disable soft-AP radio and switch to STA-only mode
// =====================================================================

void shutdownAP() {
  WiFi.persistent(true);
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
  Serial.print(F("AP IP: ")); Serial.println(WiFi.softAPIP());
  Serial.print(F("AP SSID: ")); Serial.println(myWiFiManager->getConfigPortalSSID());

  // Fast blink LED to indicate config mode
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
  }
}

void saveConfigCallback() {
  Serial.println(F("WiFiManager: Config saved"));
}
