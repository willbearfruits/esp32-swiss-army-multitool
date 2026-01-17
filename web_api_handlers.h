/*
 * ESP32 Multitool - API Handlers for Modern Web Interface
 * RESTful JSON API endpoints for dashboard communication
 */

#ifndef WEB_API_HANDLERS_H
#define WEB_API_HANDLERS_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>

// Forward declarations from main sketch
extern WebServer server;
extern char www_username[];
extern char www_password[];
extern char ota_password[];
extern SemaphoreHandle_t stateMutex;
extern SharedState sharedState;
extern Preferences preferences;

// I2C scan results cache
struct I2CDevice {
  uint8_t addr;
  const char* name;
};
std::vector<I2CDevice> i2cDevices;

// Servo and PWM state
int currentServoAngle = 90;
int currentPWMValue = 0;

/**
 * API: Get system status
 * GET /api/status
 * Returns: JSON with relay, sensor, WiFi, heap info
 */
void handleAPIStatus() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  StaticJsonDocument<512> doc;

  // Get shared state safely
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    doc["relay"] = sharedState.relayState;
    doc["sensor"] = sharedState.sensorValue;
    doc["clients"] = sharedState.wifiClients;
    doc["ip"] = sharedState.ipAddress;
    xSemaphoreGive(stateMutex);
  }

  doc["heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis();
  doc["rssi"] = WiFi.RSSI();
  doc["pwm"] = currentPWMValue;
  doc["servo"] = currentServoAngle;

  String response;
  serializeJson(doc, response);
  server.send(200, F("application/json"), response);
}

/**
 * API: Control relay
 * POST /api/relay
 * Body: {"state": true/false}
 */
void handleAPIRelay() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  bool state = doc["state"] | false;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sharedState.relayState = state;
    xSemaphoreGive(stateMutex);
  }

  server.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
}

/**
 * API: Control PWM dimmer
 * POST /api/pwm
 * Body: {"value": 0-100}
 */
void handleAPIPWM() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  int value = doc["value"] | 0;
  value = constrain(value, 0, 100);
  currentPWMValue = value;

  // Convert 0-100% to 0-255 PWM value with gamma correction
  extern uint8_t gammaCorrect(uint8_t brightness);
  uint8_t pwm = map(value, 0, 100, 0, 255);
  uint8_t corrected = gammaCorrect(pwm);

  extern const uint8_t PWM_MOSFET;
  ledcWrite(Pins::PWM_MOSFET, corrected);

  server.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
}

/**
 * API: Control servo
 * POST /api/servo
 * Body: {"angle": 0-180}
 */
void handleAPIServo() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, F("application/json"), F("{\"error\":\"Invalid JSON\"}"));
    return;
  }

  int angle = doc["angle"] | 90;
  angle = constrain(angle, 0, 180);
  currentServoAngle = angle;

  extern Servo myServo;

  static bool servoAttached = false;
  if (!servoAttached) {
    myServo.attach(Pins::SERVO);
    servoAttached = true;
  }

  myServo.write(angle);

  server.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
}

/**
 * API: I2C Bus Scanner
 * GET /api/i2c/scan
 * Returns: JSON array of detected devices
 */
void handleAPIi2cScan() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  i2cDevices.clear();

  extern SemaphoreHandle_t i2cMutex;
  extern const char* getI2CDeviceName(uint8_t addr);

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(500))) {
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        I2CDevice dev;
        dev.addr = addr;
        dev.name = getI2CDeviceName(addr);
        i2cDevices.push_back(dev);
      }
    }
    xSemaphoreGive(i2cMutex);
  }

  // Build JSON response
  String response = "{\"devices\":[";
  for (size_t i = 0; i < i2cDevices.size(); i++) {
    if (i > 0) response += ",";
    response += "{\"addr\":" + String(i2cDevices[i].addr) +
                ",\"name\":\"" + String(i2cDevices[i].name) + "\"}";
  }
  response += "]}";

  server.send(200, F("application/json"), response);
}

/**
 * API: Update passwords
 * POST /api/password
 * Body: {"current":"...", "newpass":"...", "otapass":"..."}
 */
void handleAPIPassword() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, F("text/plain"), F("Invalid JSON"));
    return;
  }

  const char* current = doc["current"];
  const char* newPass = doc["newpass"];
  const char* otaPass = doc["otapass"];

  // Verify current password
  if (strcmp(current, www_password) != 0) {
    server.send(401, F("text/plain"), F("Current password incorrect"));
    return;
  }

  // Validate length
  if (strlen(newPass) < 8 || strlen(otaPass) < 8) {
    server.send(400, F("text/plain"), F("Passwords must be at least 8 characters"));
    return;
  }

  // Save to NVS
  preferences.begin("auth", false);
  preferences.putString("pass", newPass);
  preferences.putString("otapass", otaPass);
  preferences.end();

  // Update runtime
  strcpy(www_password, newPass);
  strcpy(ota_password, otaPass);

  server.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
}

/**
 * API: MQTT Configuration
 * POST /api/mqtt
 * Body: {"server":"...", "port":1883, "client":"...", "user":"...", "pass":"..."}
 */
void handleAPIMQTT() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, F("text/plain"), F("Invalid JSON"));
    return;
  }

  // Save MQTT config to NVS
  preferences.begin("mqtt", false);
  preferences.putString("server", doc["server"] | "broker.hivemq.com");
  preferences.putUInt("port", doc["port"] | 1883);
  preferences.putString("client", doc["client"] | "ESP32_Multitool");
  preferences.putString("user", doc["user"] | "");
  preferences.putString("pass", doc["pass"] | "");
  preferences.end();

  server.send(200, F("application/json"), F("{\"status\":\"ok\"}"));
}

/**
 * API: Network information
 * GET /api/network
 */
void handleAPINetwork() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  StaticJsonDocument<256> doc;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["rssi"] = WiFi.RSSI();
  doc["channel"] = WiFi.channel();

  String response;
  serializeJson(doc, response);
  server.send(200, F("application/json"), response);
}

/**
 * API: Firmware information
 * GET /api/firmware
 */
void handleAPIFirmware() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  StaticJsonDocument<256> doc;
  doc["version"] = "2.0.0";
  doc["buildDate"] = __DATE__ " " __TIME__;
  doc["sketchSize"] = ESP.getSketchSize();
  doc["freeSpace"] = ESP.getFreeSketchSpace();
  doc["sdkVersion"] = ESP.getSdkVersion();
  doc["cpuFreq"] = ESP.getCpuFreqMHz();

  String response;
  serializeJson(doc, response);
  server.send(200, F("application/json"), response);
}

/**
 * API: WiFi Reset
 * POST /api/wifi/reset
 */
void handleAPIWiFiReset() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  server.send(200, F("application/json"), F("{\"status\":\"resetting\"}"));

  delay(1000);

  extern WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
}

/**
 * API: Reboot device
 * POST /api/reboot
 */
void handleAPIReboot() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  server.send(200, F("application/json"), F("{\"status\":\"rebooting\"}"));

  delay(1000);
  ESP.restart();
}

/**
 * OTA Update handler
 * POST /update
 */
void handleOTAUpdate() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleOTAUpdateDone() {
  if (Update.hasError()) {
    server.send(500, F("text/plain"), F("Update Failed"));
  } else {
    server.send(200, F("text/plain"), F("Update OK - Rebooting..."));
    delay(1000);
    ESP.restart();
  }
}

/**
 * Register all API routes
 * Call this from setup() after server.begin()
 */
void registerAPIHandlers() {
  // API endpoints
  server.on("/api/status", HTTP_GET, handleAPIStatus);
  server.on("/api/relay", HTTP_POST, handleAPIRelay);
  server.on("/api/pwm", HTTP_POST, handleAPIPWM);
  server.on("/api/servo", HTTP_POST, handleAPIServo);
  server.on("/api/i2c/scan", HTTP_GET, handleAPIi2cScan);
  server.on("/api/password", HTTP_POST, handleAPIPassword);
  server.on("/api/mqtt", HTTP_POST, handleAPIMQTT);
  server.on("/api/network", HTTP_GET, handleAPINetwork);
  server.on("/api/firmware", HTTP_GET, handleAPIFirmware);
  server.on("/api/wifi/reset", HTTP_POST, handleAPIWiFiReset);
  server.on("/api/reboot", HTTP_POST, handleAPIReboot);

  // OTA update endpoint
  server.on("/update", HTTP_POST, handleOTAUpdateDone, handleOTAUpdate);

  Serial.println(F("API handlers registered"));
}

#endif
