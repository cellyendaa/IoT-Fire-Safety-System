#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

// === WiFi & MQTT Config ===
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqttServer = "thingsboard.cloud";
const int mqttPort = 1883;
const char* mqttUsername = "7machkmkxfg369m89v0m";
const char* mqttClientID = "ESP32-SmokeFire-kel8";

// === PIN DEFINITIONS ===
#define BUZZERPIN 27
#define REDPIN 16
#define GREENPIN 17
#define BLUEPIN 18

// === LCD CONFIG ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === TIMING CONFIG ===
unsigned long lastDisplayUpdate = 0;
unsigned long lastLogUpdate = 0;
unsigned long lastStepUpdate = 0;
unsigned long lastMqttSend = 0;
unsigned long lastConnectionCheck = 0;

const unsigned long displayInterval = 1000;
const unsigned long logInterval = 1000;
const unsigned long stepInterval = 5000;
const unsigned long mqttInterval = 5000;
const unsigned long connectionCheckInterval = 10000;

// === OFFLINE STORAGE ===
Preferences preferences;
bool dataPending = false;

// === SYSTEM STATE ===
int page = 0;
int currentStage = 0;
bool systemCritical = false;

// Sensor data structure
struct SensorData {
  float temperature;
  float humidity;
  int smoke;
  int light;
  String status;
};

SensorData sensorData = {0, 0, 0, 0, "INITIALIZING"};

// Threshold structure
struct Threshold {
  float temp;
  float hum;
  int smoke;
  int light;
  String label;
  int r, g, b;
};

Threshold thresholds[] = {
  {30, 70, 300, 400, "SAFE", 0, 255, 0},
  {45, 85, 500, 600, "CAUTION", 255, 255, 0},
  {60, 90, 700, 800, "MODERATE DANGER", 255, 165, 0},
  {70, 95, 900, 1000, "HIGH DANGER", 255, 0, 0},
  {80, 100, 901, 1500, "CRITICAL! EVACUATE!", 128, 0, 128}
};

const int maxStage = sizeof(thresholds) / sizeof(thresholds[0]);

WiFiClient espClient;
PubSubClient client(espClient);

// ==================== FUNCTION DEFINITIONS ====================

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smoke&Fire System");
  
  for(int i=0; i<3; i++) {
    lcd.print(".");
    delay(500);
  }
  
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);
  lcd.clear();
}

void initializePins() {
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);
  setRGB(0, 0, 255);
}

void setRGB(int r, int g, int b) {
  analogWrite(REDPIN, r);
  analogWrite(GREENPIN, g);
  analogWrite(BLUEPIN, b);
}

void updateLCD() {
  lcd.clear();
  if (page == 0) {
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(sensorData.temperature, 1);
    lcd.print("C H:"); lcd.print(sensorData.humidity, 0); lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("S:"); lcd.print(sensorData.smoke);
    lcd.print(" L:"); lcd.print(sensorData.light);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Status:");
    lcd.setCursor(0, 1);
    lcd.print(sensorData.status);
    
    if (!client.connected()) {
      lcd.setCursor(13, 1);
      lcd.print("X");
    }
    
    if (dataPending) {
      lcd.setCursor(15, 1);
      lcd.print("!");
    }
  }
}

void logToSerial() {
  Serial.print("Temp: "); Serial.print(sensorData.temperature);
  Serial.print("C | Hum: "); Serial.print(sensorData.humidity);
  Serial.print("% | Smoke: "); Serial.print(sensorData.smoke);
  Serial.print(" | Light: "); Serial.print(sensorData.light);
  Serial.print(" | Status: "); Serial.println(sensorData.status);
  Serial.print("WiFi: "); Serial.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  Serial.print(" | MQTT: "); Serial.println(client.connected() ? "Connected" : "Disconnected");
}

void simulateSensors() {
  Threshold t = thresholds[currentStage];

  if (sensorData.temperature < t.temp) sensorData.temperature += 1.5;
  if (sensorData.humidity < t.hum) sensorData.humidity += 2.5;
  if (sensorData.smoke < t.smoke) sensorData.smoke += 25;
  if (sensorData.light < t.light) sensorData.light += 50;

  sensorData.temperature = min(sensorData.temperature, t.temp);
  sensorData.humidity = min(sensorData.humidity, t.hum);
  sensorData.smoke = min(sensorData.smoke, t.smoke);
  sensorData.light = min(sensorData.light, t.light);

  sensorData.status = t.label;
  setRGB(t.r, t.g, t.b);

  if (sensorData.status.startsWith("CRITICAL")) {
    tone(BUZZERPIN, 1000);
    systemCritical = true;
  } else {
    noTone(BUZZERPIN);
    systemCritical = false;
  }

  if (sensorData.temperature >= t.temp &&
      sensorData.humidity >= t.hum &&
      sensorData.smoke >= t.smoke &&
      sensorData.light >= t.light) {
    currentStage = min(currentStage + 1, maxStage - 1);
  }
}

void saveOfflineData() {
  preferences.putFloat("temp", sensorData.temperature);
  preferences.putFloat("hum", sensorData.humidity);
  preferences.putInt("smoke", sensorData.smoke);
  preferences.putInt("light", sensorData.light);
  preferences.putString("status", sensorData.status.c_str());
  preferences.putInt("stage", currentStage);
  preferences.putULong("timestamp", millis() / 1000);
  dataPending = true;
  Serial.println("Data saved to offline storage!");
}

void sendBufferedData() {
  if (!preferences.getBool("hasData", false)) return;

  DynamicJsonDocument doc(256);
  doc["temperature"] = preferences.getFloat("temp", 0);
  doc["humidity"] = preferences.getFloat("hum", 0);
  doc["smoke"] = preferences.getInt("smoke", 0);
  doc["light"] = preferences.getInt("light", 0);
  doc["status"] = preferences.getString("status", "");
  doc["stage"] = preferences.getInt("stage", 0);
  doc["ts"] = preferences.getULong("timestamp", 0);

  char buffer[256];
  serializeJson(doc, buffer);

  if (client.publish("v1/devices/me/telemetry", buffer)) {
    preferences.clear();
    preferences.putBool("hasData", false);
    dataPending = false;
    Serial.println("Buffered data sent successfully!");
  }
}

void sendTelemetry(bool force = false) {
  unsigned long now = millis();
  
  if (force || (now - lastMqttSend >= mqttInterval)) {
    if (!client.connected()) {
      saveOfflineData();
      return;
    }

    DynamicJsonDocument doc(256);
    doc["temperature"] = sensorData.temperature;
    doc["humidity"] = sensorData.humidity;
    doc["smoke"] = sensorData.smoke;
    doc["light"] = sensorData.light;
    doc["status"] = sensorData.status;
    doc["stage"] = currentStage;
    doc["ts"] = now / 1000;

    char buffer[256];
    serializeJson(doc, buffer);

    if (!client.publish("v1/devices/me/telemetry", buffer)) {
      Serial.println("Failed to publish telemetry");
      saveOfflineData();
    } else {
      lastMqttSend = now;
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String json = "";
  for (int i = 0; i < length; i++) json += (char)payload[i];

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  String method = doc["method"];
  if (method == "setBuzzer") {
    bool on = doc["params"];
    if (on) tone(BUZZERPIN, 1000);
    else noTone(BUZZERPIN);
    
    String responseTopic = "v1/devices/me/rpc/response/";
    responseTopic += doc["id"].as<int>();
    String response = "{\"method\":\"setBuzzer\",\"params\":";
    response += on ? "true}" : "false}";
    client.publish(responseTopic.c_str(), response.c_str());
  }
}

void reconnect() {
  Serial.println("Attempting MQTT connection...");
  
  if (client.connect(mqttClientID, mqttUsername, NULL)) {
    Serial.println("Connected to ThingsBoard!");
    client.subscribe("v1/devices/me/rpc/request/+");
    
    DynamicJsonDocument attrDoc(256);
    attrDoc["firmware"] = "v2.0";
    attrDoc["model"] = "ESP32-SmokeFire";
    char attrBuffer[256];
    serializeJson(attrDoc, attrBuffer);
    client.publish("v1/devices/me/attributes", attrBuffer);

    sendBufferedData(); // Coba kirim data yang tertahan
  } else {
    Serial.print("MQTT connection failed, rc=");
    Serial.println(client.state());
    
    lcd.clear();
    lcd.print("MQTT Error:");
    lcd.setCursor(0, 1);
    lcd.print("RC=");
    lcd.print(client.state());
    delay(2000);
  }
}

void checkConnections() {
  static int wifiRetries = 0;
  static int mqttRetries = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if(wifiRetries++ > 5) {
      Serial.println("WiFi reconnection failed, rebooting...");
      ESP.restart();
    }
    Serial.println("WiFi disconnected! Reconnecting...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!client.connected()) {
    if(mqttRetries++ > 3) {
      wifiRetries = 0;
      return;
    }
    reconnect();
  } else {
    mqttRetries = 0;
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "reset") {
      currentStage = 0;
      sensorData = {0, 0, 0, 0, "RESET"};
      setRGB(0, 0, 255);
      Serial.println("System reset!");
    } else if (cmd == "status") {
      Serial.println("=== SYSTEM STATUS ===");
      Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
      Serial.println("MQTT: " + String(client.connected() ? "Connected" : "Disconnected"));
      Serial.println("Current Stage: " + String(currentStage));
      Serial.println("Pending Data: " + String(dataPending ? "Yes" : "No"));
    } else if (cmd == "simulate_offline") {
      Serial.println("Simulating offline mode...");
      while(true) { // Blocking loop untuk simulasi offline
        saveOfflineData();
        delay(1000);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  initializePins();
  initializeLCD();

  // Inisialisasi penyimpanan offline
  preferences.begin("sensor-data", false);
  if (preferences.getBool("hasData", false)) {
    dataPending = true;
    Serial.println("Found pending offline data!");
  }

  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  reconnect();

  lastStepUpdate = millis();
}

void loop() {
  unsigned long now = millis();

  client.loop();
  checkConnections();
  handleSerialCommands();

  if (now - lastStepUpdate >= stepInterval && currentStage < maxStage) {
    lastStepUpdate = now;
    simulateSensors();
  }

  if (now - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = now;
    updateLCD();
    page = (page + 1) % 2;
  }

  if (now - lastLogUpdate >= logInterval) {
    lastLogUpdate = now;
    logToSerial();
  }

  sendTelemetry();

  if (systemCritical && now - lastStepUpdate > 60000) {
    Serial.println("Critical state for 1min - entering deep sleep");
    esp_sleep_enable_timer_wakeup(30 * 1000000);
    esp_deep_sleep_start();
  }
}