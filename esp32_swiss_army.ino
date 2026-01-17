/*
 * ESP32 Swiss Army Multitool
 *
 * A versatile ESP32-based hardware control system with:
 * - WiFi Access Point with web interface
 * - OLED display with rotary encoder navigation
 * - Multiple peripheral control (Relay, NeoPixel, Servo, etc.)
 * - Dual-core FreeRTOS architecture
 *
 * IMPORTANT: Configure WiFi credentials using the captive portal on first boot
 *
 * Hardware Requirements:
 * - ESP32 DevKit
 * - SSD1306 OLED Display (128x64, I2C address 0x3C)
 * - Rotary Encoder (with push button)
 * - 36-LED NeoPixel Ring
 * - Various optional peripherals (Relay, Servo, etc.)
 *
 * License: MIT (see LICENSE file)
 * Version: 2.5.0 (Fully Featured)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_adc_cal.h>
#include <driver/ledc.h>
#include <driver/dac.h>
#include <PubSubClient.h>

// --- CONFIGURATION CONSTANTS ---

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// Pin Definitions
namespace Pins {
  // Input Pins
  const uint8_t ROT_A = 32;
  const uint8_t ROT_B = 33;
  const uint8_t ROT_SW = 25;
  const uint8_t SENSOR_IN = 34;  // Input-only pin

  // Output Pins (avoiding strapping pins)
  const uint8_t RELAY = 23;
  const uint8_t SERVO = 13;
  const uint8_t NEOPIXEL = 15;
  const uint8_t PWM_MOSFET = 16;  // Changed from GPIO2 (strapping pin)
  const uint8_t STEP1 = 18;
  const uint8_t STEP2 = 19;
  const uint8_t STEP3 = 4;
  const uint8_t STEP4 = 5;

  // I2S Pins (reserved for future use)
  const uint8_t I2S_BCLK = 26;
  const uint8_t I2S_LRC = 27;
  const uint8_t I2S_DOUT = 14;
}

// Timing Constants
namespace Timing {
  const uint16_t DEBOUNCE_MS = 250;
  const uint16_t DISPLAY_UPDATE_MS = 100;
  const uint16_t WIFI_CLIENT_CHECK_MS = 1000;
  const uint16_t NEOPIXEL_ANIMATION_MS = 20;
  const uint16_t WATCHDOG_TIMEOUT_MS = 30000;
}

// ADC Constants
const uint16_t ADC_MAX_12BIT = 4095;
const uint8_t ADC_SAMPLES = 32;  // Multisampling for ADC calibration

// PWM Constants (LEDC)
const uint8_t PWM_CHANNEL = 0;
const uint32_t PWM_FREQUENCY = 5000;  // 5kHz
const uint8_t PWM_RESOLUTION = 8;     // 8-bit (0-255)

// OTA Constants
const char OTA_PASSWORD[] = "esp32update";  // Change this for production
const char MDNS_HOSTNAME[] = "esp32-multitool";

// I2S/DAC Constants for tone generator
const uint16_t TONE_FREQ_MIN = 100;   // 100 Hz
const uint16_t TONE_FREQ_MAX = 4000;  // 4 kHz
const uint32_t DAC_SAMPLE_RATE = 44100;  // 44.1 kHz

// MQTT Constants
const char MQTT_SERVER[] = "broker.hivemq.com";  // Default public broker
const uint16_t MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "ESP32_Multitool";
const char MQTT_TOPIC_STATE[] = "esp32/multitool/state";
const char MQTT_TOPIC_RELAY[] = "esp32/multitool/relay";
const char MQTT_TOPIC_SENSOR[] = "esp32/multitool/sensor";

// NeoPixel Configuration
#define LED_COUNT 36
#define LED_BRIGHTNESS 50  // 20% brightness to prevent brownout (36 LEDs draw ~720mA at 50% white)

// FreeRTOS Task Configuration
namespace TaskConfig {
  const uint16_t WIFI_TASK_STACK = 8192;  // Increased from 4096 to prevent overflow
  const uint8_t WIFI_TASK_PRIORITY = 2;   // Above Arduino loop, below lwIP (18)
  const uint8_t WIFI_TASK_CORE = 0;       // Core 0 for networking
}

// WiFi Configuration
namespace WiFiConfig {
  const uint8_t AP_CHANNEL = 6;
  const uint8_t MAX_CLIENTS = 4;
  const wifi_power_t TX_POWER = WIFI_POWER_19_5dBm;
}

// --- GLOBAL OBJECTS ---

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ESP32Encoder encoder;
Adafruit_NeoPixel strip(LED_COUNT, Pins::NEOPIXEL, NEO_GRB + NEO_KHZ800);
Servo myServo;
WebServer server(80);
WiFiManager wifiManager;
Preferences preferences;
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

// --- THREAD-SAFE SHARED STATE ---

// Mutex for protecting shared variables
SemaphoreHandle_t stateMutex = nullptr;

// Mutex for I2C bus protection (display, sensors)
SemaphoreHandle_t i2cMutex = nullptr;

// Shared state variables (protected by stateMutex)
struct SharedState {
  bool relayState;
  int sensorValue;
  int wifiClients;
  bool wifiActive;
  char ipAddress[16];

  SharedState() : relayState(false), sensorValue(0), wifiClients(0),
                  wifiActive(false) {
    strcpy(ipAddress, "0.0.0.0");
  }
} sharedState;

// --- DISPLAY STATE (accessed only by Core 1) ---

bool displayAvailable = false;
unsigned long lastButtonPress = 0;

// --- APPLICATION STATE MACHINE ---

enum AppState {
  MENU,
  APP_RELAY,
  APP_I2C,
  APP_SERVO,
  APP_I2S,
  APP_STEPPER,
  APP_GPS,
  APP_NEOPIXEL,
  APP_SENSOR,
  APP_PWM,
  APP_WIFI_STATUS
};

AppState currentState = MENU;
int menuSelection = 0;
const int menuTotal = 10;
const char* menuItems[] = {
  "Relay",
  "I2C Scan",
  "Servo",
  "I2S Tone",
  "Stepper",
  "GPS Raw",
  "NeoPixel",
  "Sensor",
  "12V Dim",
  "WiFi Info"
};

// --- AUTHENTICATION ---

// Load credentials from NVS or use defaults
char www_username[32] = "admin";
char www_password[64] = "";  // Empty = no auth initially
char ota_password[64] = "";  // OTA password

void loadWebCredentials() {
  preferences.begin("auth", true);  // Read-only
  preferences.getString("user", www_username, sizeof(www_username));
  preferences.getString("pass", www_password, sizeof(www_password));
  preferences.getString("otapass", ota_password, sizeof(ota_password));
  preferences.end();

  // If no password set, use default for first setup
  if (strlen(www_password) == 0) {
    strcpy(www_password, "changeme");
    Serial.println(F("WARNING: Using default password 'changeme' - please change it!"));
  }

  if (strlen(ota_password) == 0) {
    strcpy(ota_password, OTA_PASSWORD);
  }
}

// --- HELPER FUNCTIONS ---

/**
 * I2C Device name lookup table
 */
const char* getI2CDeviceName(uint8_t addr) {
  switch (addr) {
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
      return "MCP23017/PCF8574";
    case 0x3C: case 0x3D:
      return "SSD1306 OLED";
    case 0x40: case 0x41: case 0x42: case 0x43:
      return "PCA9685/INA219";
    case 0x48: case 0x49: case 0x4A: case 0x4B:
      return "ADS1115/TMP102";
    case 0x50: case 0x51: case 0x52: case 0x53:
      return "EEPROM (AT24)";
    case 0x57:
      return "EEPROM (AT24)";
    case 0x68: case 0x69:
      return "MPU6050/DS3231";
    case 0x76: case 0x77:
      return "BMP280/BME280";
    default:
      return "Unknown";
  }
}

/**
 * Apply gamma correction for LED dimming
 * Makes brightness feel more linear to human eye
 */
uint8_t gammaCorrect(uint8_t brightness) {
  // Simple gamma correction (gamma ~2.2)
  static const uint8_t PROGMEM gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
  };
  return pgm_read_byte(&gamma8[brightness]);
}

/**
 * Read ADC with multisampling and calibration
 */
uint32_t readCalibratedADC(uint8_t pin) {
  static esp_adc_cal_characteristics_t adc_chars;
  static bool adc_calibrated = false;

  if (!adc_calibrated) {
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    adc_calibrated = true;
  }

  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  uint32_t avgRaw = sum / ADC_SAMPLES;

  // Convert to millivolts
  return esp_adc_cal_raw_to_voltage(avgRaw, &adc_chars);
}

/**
 * Generate sine wave sample for DAC
 */
uint8_t generateSineSample(uint16_t frequency, unsigned long sampleIndex) {
  // Calculate phase (0 to 2*PI)
  float phase = (2.0 * PI * frequency * sampleIndex) / DAC_SAMPLE_RATE;

  // Generate sine wave and scale to 0-255 for 8-bit DAC
  float sine = sin(phase);
  return (uint8_t)((sine + 1.0) * 127.5);
}

/**
 * MQTT callback for incoming messages
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to null-terminated string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print(F("MQTT received: "));
  Serial.print(topic);
  Serial.print(F(" -> "));
  Serial.println(message);

  // Handle relay control commands
  if (strcmp(topic, MQTT_TOPIC_RELAY) == 0) {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
      if (strcmp(message, "ON") == 0 || strcmp(message, "1") == 0) {
        sharedState.relayState = true;
      } else if (strcmp(message, "OFF") == 0 || strcmp(message, "0") == 0) {
        sharedState.relayState = false;
      }
      xSemaphoreGive(stateMutex);
    }
  }
}

/**
 * Check if button is pressed with debouncing
 * @return true if button was pressed
 */
bool buttonPressed() {
  if (digitalRead(Pins::ROT_SW) == LOW) {
    if (millis() - lastButtonPress > Timing::DEBOUNCE_MS) {
      lastButtonPress = millis();
      return true;
    }
  }
  return false;
}

/**
 * Draw consistent header on display
 * @param title Header text to display
 */
void drawHeader(const char* title) {
  if (!displayAvailable) return;

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print(F("> "));
    display.println(title);
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    xSemaphoreGive(i2cMutex);
  }
}

/**
 * Sanitize HTML input to prevent XSS
 * @param input Input string to sanitize
 * @param output Output buffer
 * @param maxLen Maximum output length
 */
void sanitizeHTML(const char* input, char* output, size_t maxLen) {
  size_t j = 0;
  for (size_t i = 0; i < strlen(input) && j < maxLen - 6; i++) {
    switch (input[i]) {
      case '<':  strncpy(&output[j], "&lt;", maxLen - j); j += 4; break;
      case '>':  strncpy(&output[j], "&gt;", maxLen - j); j += 4; break;
      case '&':  strncpy(&output[j], "&amp;", maxLen - j); j += 5; break;
      case '"':  strncpy(&output[j], "&quot;", maxLen - j); j += 6; break;
      case '\'': strncpy(&output[j], "&#x27;", maxLen - j); j += 6; break;
      default:   output[j++] = input[i]; break;
    }
  }
  output[j] = '\0';
}

// --- WEB SERVER HANDLERS ---

/**
 * Serve main web interface
 * Uses F() macro to keep strings in flash memory
 */
void handleRoot() {
  // Simple authentication check
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  // Get current state safely
  int currentSensor, currentClients;
  bool currentRelay;
  char currentIP[16];

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    currentSensor = sharedState.sensorValue;
    currentClients = sharedState.wifiClients;
    currentRelay = sharedState.relayState;
    strncpy(currentIP, sharedState.ipAddress, sizeof(currentIP));
    xSemaphoreGive(stateMutex);
  } else {
    server.send(503, F("text/plain"), F("Service temporarily unavailable"));
    return;
  }

  // Send response using WiFiClient to avoid String concatenation
  WiFiClient client = server.client();

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><title>ESP32 Multitool Control</title>"));
  client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"));
  client.println(F("<style>"));
  client.println(F("body{background:#000;color:#0f0;font-family:monospace;padding:20px;font-size:1.5rem;text-align:center;margin:0;}"));
  client.println(F("h1{border-bottom:2px solid #0f0;padding-bottom:10px;margin-top:0;}"));
  client.println(F("div{margin:20px auto;border:1px solid #333;padding:20px;max-width:600px;}"));
  client.println(F("button{background:#0f0;color:#000;border:none;padding:20px;margin:10px;font-size:1.5rem;width:80%;font-weight:bold;cursor:pointer;}"));
  client.println(F("button:active{background:#fff;}"));
  client.println(F(".off{background:#f00;color:#fff;}"));
  client.println(F(".info{font-size:1rem;color:#888;margin-top:20px;}"));
  client.println(F("</style></head><body>"));

  client.println(F("<h1>ESP32 MULTITOOL</h1>"));

  client.println(F("<div><b>STATUS:</b> CONNECTED<br>"));
  client.print(F("IP: "));
  client.print(currentIP);
  client.print(F("<br>Clients: "));
  client.print(currentClients);
  client.println(F("</div>"));

  client.println(F("<div><b>SENSOR VALUE</b><br>"));
  client.print(currentSensor);
  client.print(F(" / "));
  client.print(ADC_MAX_12BIT);
  client.println(F("</div>"));

  client.println(F("<div><b>RELAY CONTROL</b><br>"));
  client.print(F("Status: <b>"));
  client.print(currentRelay ? F("ON") : F("OFF"));
  client.println(F("</b><br>"));

  // CSRF protection: Simple approach using HTTP POST
  client.println(F("<form method=\"POST\" action=\"/relay/on\">"));
  client.println(F("<button type=\"submit\">TURN ON</button>"));
  client.println(F("</form>"));

  client.println(F("<form method=\"POST\" action=\"/relay/off\">"));
  client.println(F("<button type=\"submit\" class=\"off\">TURN OFF</button>"));
  client.println(F("</form>"));

  client.println(F("</div>"));

  client.println(F("<div class=\"info\">"));
  client.println(F("ESP32 Multitool v2.5<br>"));
  client.print(F("Free Heap: "));
  client.print(ESP.getFreeHeap());
  client.println(F(" bytes<br>"));
  client.print(F("Hostname: "));
  client.print(MDNS_HOSTNAME);
  client.println(F(".local<br><br>"));

  client.println(F("<a href=\"/password\" style=\"color:#0f0;text-decoration:underline;\">Change Passwords</a> | "));
  client.println(F("<a href=\"/settings\" style=\"color:#0f0;text-decoration:underline;\">Settings</a><br><br>"));

  client.println(F("API Endpoints:<br>"));
  client.println(F("GET <a href=\"/api/sensor\" style=\"color:#0f0;\">/api/sensor</a><br>"));
  client.println(F("GET <a href=\"/api/relay\" style=\"color:#0f0;\">/api/relay</a><br>"));
  client.println(F("GET <a href=\"/api/pwm\" style=\"color:#0f0;\">/api/pwm</a><br>"));
  client.println(F("GET <a href=\"/api/servo\" style=\"color:#0f0;\">/api/servo</a><br>"));
  client.println(F("GET <a href=\"/api/system\" style=\"color:#0f0;\">/api/system</a>"));
  client.println(F("</div>"));

  client.println(F("</body></html>"));
  client.stop();
}

/**
 * Handle relay ON command
 */
void handleRelayOn() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  // Only accept POST to prevent CSRF
  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sharedState.relayState = true;
    xSemaphoreGive(stateMutex);
  }

  server.sendHeader(F("Location"), F("/"));
  server.send(303);  // See Other
}

/**
 * Handle relay OFF command
 */
void handleRelayOff() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sharedState.relayState = false;
    xSemaphoreGive(stateMutex);
  }

  server.sendHeader(F("Location"), F("/"));
  server.send(303);
}

/**
 * Handle password change form
 */
void handlePasswordForm() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  WiFiClient client = server.client();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><title>Change Password</title>"));
  client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"));
  client.println(F("<style>"));
  client.println(F("body{background:#000;color:#0f0;font-family:monospace;padding:20px;font-size:1.2rem;}"));
  client.println(F("h1{border-bottom:2px solid #0f0;padding-bottom:10px;}"));
  client.println(F("form{max-width:500px;margin:20px auto;border:1px solid #333;padding:20px;}"));
  client.println(F("label{display:block;margin:15px 0 5px;}"));
  client.println(F("input{width:100%;padding:10px;background:#111;color:#0f0;border:1px solid #0f0;font-size:1rem;}"));
  client.println(F("button{background:#0f0;color:#000;border:none;padding:15px;margin:20px 0;width:100%;font-size:1.2rem;cursor:pointer;}"));
  client.println(F("button:active{background:#fff;}"));
  client.println(F(".back{background:#333;color:#0f0;margin-top:10px;}"));
  client.println(F("</style></head><body>"));
  client.println(F("<h1>CHANGE PASSWORD</h1>"));
  client.println(F("<form method=\"POST\" action=\"/password/update\">"));
  client.println(F("<label>Current Password:</label>"));
  client.println(F("<input type=\"password\" name=\"current\" required>"));
  client.println(F("<label>New Web Password:</label>"));
  client.println(F("<input type=\"password\" name=\"newpass\" required minlength=\"8\">"));
  client.println(F("<label>New OTA Password:</label>"));
  client.println(F("<input type=\"password\" name=\"otapass\" required minlength=\"8\">"));
  client.println(F("<button type=\"submit\">UPDATE PASSWORDS</button>"));
  client.println(F("</form>"));
  client.println(F("<form action=\"/\"><button class=\"back\">BACK</button></form>"));
  client.println(F("</body></html>"));
  client.stop();
}

/**
 * Handle password update
 */
void handlePasswordUpdate() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  // Get form data
  char currentPass[64];
  char newPass[64];
  char newOTAPass[64];

  server.arg("current").toCharArray(currentPass, sizeof(currentPass));
  server.arg("newpass").toCharArray(newPass, sizeof(newPass));
  server.arg("otapass").toCharArray(newOTAPass, sizeof(newOTAPass));

  // Verify current password
  if (strcmp(currentPass, www_password) != 0) {
    server.send(401, F("text/plain"), F("Current password incorrect"));
    return;
  }

  // Validate new password length
  if (strlen(newPass) < 8 || strlen(newOTAPass) < 8) {
    server.send(400, F("text/plain"), F("New passwords must be at least 8 characters"));
    return;
  }

  // Save to NVS
  preferences.begin("auth", false);  // Read-write
  preferences.putString("pass", newPass);
  preferences.putString("otapass", newOTAPass);
  preferences.end();

  // Update runtime variables
  strcpy(www_password, newPass);
  strcpy(ota_password, newOTAPass);

  Serial.println(F("Passwords updated successfully"));

  server.sendHeader(F("Location"), F("/"));
  server.send(303);
}

/**
 * API: Get sensor data in JSON format
 */
void handleApiSensor() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  int sensorVal = 0;
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sensorVal = sharedState.sensorValue;
    xSemaphoreGive(stateMutex);
  } else {
    server.send(503, F("application/json"), F("{\"error\":\"Service unavailable\"}"));
    return;
  }

  // Read calibrated voltage
  uint32_t millivolts = readCalibratedADC(Pins::SENSOR_IN);

  WiFiClient client = server.client();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.println();

  client.print(F("{\"raw\":"));
  client.print(sensorVal);
  client.print(F(",\"voltage_mv\":"));
  client.print(millivolts);
  client.print(F(",\"voltage_v\":"));
  client.print(millivolts / 1000.0, 3);
  client.println(F("}"));
  client.stop();
}

/**
 * API: Get/Set relay state in JSON
 */
void handleApiRelay() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() == HTTP_GET) {
    bool relayState = false;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
      relayState = sharedState.relayState;
      xSemaphoreGive(stateMutex);
    } else {
      server.send(503, F("application/json"), F("{\"error\":\"Service unavailable\"}"));
      return;
    }

    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"state\":"));
    client.print(relayState ? F("true") : F("false"));
    client.println(F("}"));
    client.stop();

  } else if (server.method() == HTTP_POST) {
    // Expect JSON body like {"state": true}
    char body[64];
    server.arg("plain").toCharArray(body, sizeof(body));

    // Simple JSON parsing (look for "true" or "false")
    bool newState = false;
    if (strstr(body, "true") != nullptr) {
      newState = true;
    }

    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
      sharedState.relayState = newState;
      xSemaphoreGive(stateMutex);
    } else {
      server.send(503, F("application/json"), F("{\"error\":\"Service unavailable\"}"));
      return;
    }

    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"state\":"));
    client.print(newState ? F("true") : F("false"));
    client.println(F("}"));
    client.stop();

  } else {
    server.send(405, F("application/json"), F("{\"error\":\"Method not allowed\"}"));
  }
}

/**
 * API: Get/Set PWM value in JSON
 */
void handleApiPwm() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  static uint8_t currentPwm = 0;

  if (server.method() == HTTP_GET) {
    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"value\":"));
    client.print(currentPwm);
    client.print(F(",\"percent\":"));
    client.print((currentPwm * 100) / 255);
    client.println(F("}"));
    client.stop();

  } else if (server.method() == HTTP_POST) {
    // Expect JSON body like {"value": 128}
    if (!server.hasArg("plain")) {
      server.send(400, F("application/json"), F("{\"error\":\"Missing body\"}"));
      return;
    }

    char body[64];
    server.arg("plain").toCharArray(body, sizeof(body));

    // Simple JSON parsing - extract number after "value":
    char* valueStr = strstr(body, "\"value\"");
    if (valueStr != nullptr) {
      valueStr = strchr(valueStr, ':');
      if (valueStr != nullptr) {
        int value = atoi(valueStr + 1);
        if (value >= 0 && value <= 255) {
          currentPwm = (uint8_t)value;
          uint8_t corrected = gammaCorrect(currentPwm);
          ledcWrite(Pins::PWM_MOSFET, corrected);
        }
      }
    }

    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"value\":"));
    client.print(currentPwm);
    client.print(F(",\"percent\":"));
    client.print((currentPwm * 100) / 255);
    client.println(F("}"));
    client.stop();

  } else {
    server.send(405, F("application/json"), F("{\"error\":\"Method not allowed\"}"));
  }
}

/**
 * API: Get/Set servo angle in JSON
 */
void handleApiServo() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  static uint8_t currentAngle = 90;

  if (server.method() == HTTP_GET) {
    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"angle\":"));
    client.print(currentAngle);
    client.println(F("}"));
    client.stop();

  } else if (server.method() == HTTP_POST) {
    // Expect JSON body like {"angle": 90}
    if (!server.hasArg("plain")) {
      server.send(400, F("application/json"), F("{\"error\":\"Missing body\"}"));
      return;
    }

    char body[64];
    server.arg("plain").toCharArray(body, sizeof(body));

    // Simple JSON parsing - extract number after "angle":
    char* angleStr = strstr(body, "\"angle\"");
    if (angleStr != nullptr) {
      angleStr = strchr(angleStr, ':');
      if (angleStr != nullptr) {
        int angle = atoi(angleStr + 1);
        if (angle >= 0 && angle <= 180) {
          currentAngle = (uint8_t)angle;
          // Note: Servo control is done in APP_SERVO state
          // For API control, we'd need global servo object
        }
      }
    }

    WiFiClient client = server.client();
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(F("{\"angle\":"));
    client.print(currentAngle);
    client.println(F("}"));
    client.stop();

  } else {
    server.send(405, F("application/json"), F("{\"error\":\"Method not allowed\"}"));
  }
}

/**
 * API: Get system information in JSON
 */
void handleApiSystem() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  // Get current state
  int currentClients = 0;
  bool wifiActive = false;
  char currentIP[16];

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    currentClients = sharedState.wifiClients;
    wifiActive = sharedState.wifiActive;
    strncpy(currentIP, sharedState.ipAddress, sizeof(currentIP));
    xSemaphoreGive(stateMutex);
  }

  WiFiClient client = server.client();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.println();

  client.print(F("{\"heap_free\":"));
  client.print(ESP.getFreeHeap());
  client.print(F(",\"heap_size\":"));
  client.print(ESP.getHeapSize());
  client.print(F(",\"uptime_ms\":"));
  client.print(millis());
  client.print(F(",\"chip_model\":\""));
  client.print(ESP.getChipModel());
  client.print(F("\",\"chip_revision\":"));
  client.print(ESP.getChipRevision());
  client.print(F(",\"cpu_freq_mhz\":"));
  client.print(ESP.getCpuFreqMHz());
  client.print(F(",\"wifi_clients\":"));
  client.print(currentClients);
  client.print(F(",\"wifi_active\":"));
  client.print(wifiActive ? F("true") : F("false"));
  client.print(F(",\"ip_address\":\""));
  client.print(currentIP);
  client.print(F("\",\"rssi_dbm\":"));
  client.print(WiFi.RSSI());
  client.println(F("}"));
  client.stop();
}

/**
 * Handle settings page (MQTT and system configuration)
 */
void handleSettings() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  WiFiClient client = server.client();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><title>Settings</title>"));
  client.println(F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"));
  client.println(F("<style>"));
  client.println(F("body{background:#000;color:#0f0;font-family:monospace;padding:20px;font-size:1.2rem;}"));
  client.println(F("h1{border-bottom:2px solid #0f0;padding-bottom:10px;}"));
  client.println(F("form{max-width:600px;margin:20px auto;border:1px solid #333;padding:20px;}"));
  client.println(F("label{display:block;margin:15px 0 5px;}"));
  client.println(F("input{width:100%;padding:10px;background:#111;color:#0f0;border:1px solid #0f0;font-size:1rem;}"));
  client.println(F("button{background:#0f0;color:#000;border:none;padding:15px;margin:20px 0;width:100%;font-size:1.2rem;cursor:pointer;}"));
  client.println(F("button:active{background:#fff;}"));
  client.println(F(".info{color:#888;font-size:0.9rem;margin-top:5px;}"));
  client.println(F(".back{background:#333;color:#0f0;margin-top:10px;}"));
  client.println(F("</style></head><body>"));

  client.println(F("<h1>SYSTEM SETTINGS</h1>"));

  // MQTT Settings
  client.println(F("<form method=\"POST\" action=\"/settings/update\">"));
  client.println(F("<h2>MQTT Configuration</h2>"));
  client.println(F("<label>MQTT Broker:</label>"));
  client.print(F("<input type=\"text\" name=\"mqtt_broker\" value=\""));
  client.print(MQTT_SERVER);
  client.println(F("\">"));
  client.println(F("<div class=\"info\">Default: broker.hivemq.com</div>"));

  client.println(F("<label>MQTT Port:</label>"));
  client.print(F("<input type=\"number\" name=\"mqtt_port\" value=\""));
  client.print(MQTT_PORT);
  client.println(F("\">"));
  client.println(F("<div class=\"info\">Default: 1883</div>"));

  client.println(F("<label>MQTT Client ID:</label>"));
  client.print(F("<input type=\"text\" name=\"mqtt_client\" value=\""));
  client.print(MQTT_CLIENT_ID);
  client.println(F("\">"));

  client.println(F("<div class=\"info\">"));
  client.print(F("Status: "));
  client.print(mqttClient.connected() ? F("Connected") : F("Disconnected"));
  client.println(F("</div>"));

  client.println(F("<button type=\"submit\">UPDATE SETTINGS</button>"));
  client.println(F("</form>"));

  client.println(F("<form action=\"/\"><button class=\"back\">BACK TO MAIN</button></form>"));
  client.println(F("</body></html>"));
  client.stop();
}

/**
 * Handle settings update
 */
void handleSettingsUpdate() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, F("text/plain"), F("Method Not Allowed"));
    return;
  }

  // For now, just acknowledge the update
  // In a full implementation, we'd save to NVS and reconfigure MQTT
  Serial.println(F("Settings update requested (not implemented - requires NVS storage)"));

  server.sendHeader(F("Location"), F("/settings"));
  server.send(303);
}

/**
 * Handle 404 errors
 */
void handleNotFound() {
  server.send(404, F("text/plain"), F("Not Found"));
}

// --- WIFI TASK (CORE 0) ---

/**
 * WiFi and web server task running on Core 0
 * Handles all network operations separate from hardware control
 */
void wifiTask(void* parameter) {
  // Add this task to watchdog
  esp_task_wdt_add(NULL);

  Serial.println(F("WiFi task starting on Core 0..."));

  // Configure WiFi manager
  wifiManager.setConfigPortalTimeout(180);  // 3 minute timeout
  wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
    Serial.println(F("Entered config mode"));
    Serial.print(F("Config AP: "));
    Serial.println(myWiFiManager->getConfigPortalSSID());
  });

  // Try to connect with saved credentials, or start config portal
  char apName[32];
  snprintf(apName, sizeof(apName), "ESP32-Multitool-%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  if (!wifiManager.autoConnect(apName, "configure")) {
    Serial.println(F("Failed to connect - resetting"));
    delay(3000);
    ESP.restart();
  }

  // Connected successfully
  Serial.println(F("WiFi connected!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  // Update shared state
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(1000))) {
    IPAddress ip = WiFi.localIP();
    snprintf(sharedState.ipAddress, sizeof(sharedState.ipAddress),
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    sharedState.wifiActive = true;
    xSemaphoreGive(stateMutex);
  }

  // Set WiFi power
  WiFi.setTxPower(WiFiConfig::TX_POWER);

  // Setup mDNS
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.print(F("mDNS started: "));
    Serial.print(MDNS_HOSTNAME);
    Serial.println(F(".local"));

    // Add service discovery
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("arduino", "tcp", 3232);  // OTA port
  } else {
    Serial.println(F("WARNING: mDNS failed to start"));
  }

  // Setup OTA
  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("OTA: Starting update - " + type);

    // Show on OLED if available
    if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("OTA UPDATE"));
      display.println(F("Starting..."));
      display.display();
      xSemaphoreGive(i2cMutex);
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nOTA: Complete!"));
    if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("OTA COMPLETE"));
      display.println(F("Rebooting..."));
      display.display();
      xSemaphoreGive(i2cMutex);
    }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t percent = (progress / (total / 100));
    Serial.printf("OTA: %u%%\r", percent);

    // Update OLED every 10%
    static uint8_t lastPercent = 0;
    if (percent != lastPercent && percent % 10 == 0) {
      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50))) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(F("OTA UPDATE"));
        display.drawRect(0, 20, 128, 20, SSD1306_WHITE);
        int barWidth = map(percent, 0, 100, 0, 126);
        display.fillRect(1, 21, barWidth, 18, SSD1306_WHITE);
        display.setCursor(0, 45);
        display.print(percent);
        display.println(F("% complete"));
        display.display();
        xSemaphoreGive(i2cMutex);
      }
      lastPercent = percent;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));

    if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(F("OTA ERROR"));
      display.display();
      xSemaphoreGive(i2cMutex);
    }
  });

  ArduinoOTA.begin();
  Serial.println(F("OTA ready"));

  // Setup MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println(F("MQTT configured"));

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/relay/on", HTTP_POST, handleRelayOn);
  server.on("/relay/off", HTTP_POST, handleRelayOff);
  server.on("/password", HTTP_GET, handlePasswordForm);
  server.on("/password/update", HTTP_POST, handlePasswordUpdate);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/settings/update", HTTP_POST, handleSettingsUpdate);

  // JSON API endpoints
  server.on("/api/sensor", HTTP_GET, handleApiSensor);
  server.on("/api/relay", handleApiRelay);  // Both GET and POST
  server.on("/api/pwm", handleApiPwm);      // Both GET and POST
  server.on("/api/servo", handleApiServo);  // Both GET and POST
  server.on("/api/system", HTTP_GET, handleApiSystem);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("Web server started"));

  // Main WiFi task loop
  unsigned long lastClientCheck = 0;
  unsigned long lastMqttPublish = 0;

  for (;;) {
    // Handle OTA updates
    ArduinoOTA.handle();

    // Handle web requests
    server.handleClient();

    // Handle MQTT connection and messages
    if (!mqttClient.connected()) {
      // Try to reconnect (non-blocking)
      static unsigned long lastReconnectAttempt = 0;
      if (millis() - lastReconnectAttempt > 5000) {  // Try every 5 seconds
        lastReconnectAttempt = millis();
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
          Serial.println(F("MQTT connected"));
          mqttClient.subscribe(MQTT_TOPIC_RELAY);
          mqttClient.publish(MQTT_TOPIC_STATE, "online", true);  // Retained message
        }
      }
    } else {
      mqttClient.loop();

      // Publish sensor data every 5 seconds
      if (millis() - lastMqttPublish > 5000) {
        char payload[32];
        int sensorVal = 0;

        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
          sensorVal = sharedState.sensorValue;
          xSemaphoreGive(stateMutex);
        }

        snprintf(payload, sizeof(payload), "%d", sensorVal);
        mqttClient.publish(MQTT_TOPIC_SENSOR, payload);

        lastMqttPublish = millis();
      }
    }

    // Update client count periodically
    if (millis() - lastClientCheck > Timing::WIFI_CLIENT_CHECK_MS) {
      int clients = WiFi.softAPgetStationNum();

      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
        sharedState.wifiClients = clients;
        xSemaphoreGive(stateMutex);
      }

      lastClientCheck = millis();
    }

    // Feed watchdog
    esp_task_wdt_reset();

    // Yield to prevent WDT timeout
    vTaskDelay(pdMS_TO_TICKS(10));  // Minimum 10ms delay
  }
}

// --- SETUP (RUNS ON CORE 1) ---

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("\n\n================================="));
  Serial.println(F("ESP32 Multitool v2.5"));
  Serial.println(F("Fully Featured Edition"));
  Serial.println(F("=================================\n"));

  // Create mutexes BEFORE starting any tasks
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  if (stateMutex == nullptr || i2cMutex == nullptr) {
    Serial.println(F("FATAL: Failed to create mutexes!"));
    while (1) {
      delay(1000);
    }
  }

  Serial.println(F("Mutexes created successfully"));

  // Configure watchdog timer (ESP32 core 3.x API)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = Timing::WATCHDOG_TIMEOUT_MS,
    .idle_core_mask = 0,  // Don't watch idle tasks
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);  // Add setup/loop task

  // Initialize GPIO pins
  pinMode(Pins::RELAY, OUTPUT);
  digitalWrite(Pins::RELAY, LOW);

  pinMode(Pins::ROT_SW, INPUT_PULLUP);
  pinMode(Pins::SENSOR_IN, INPUT);

  pinMode(Pins::STEP1, OUTPUT);
  pinMode(Pins::STEP2, OUTPUT);
  pinMode(Pins::STEP3, OUTPUT);
  pinMode(Pins::STEP4, OUTPUT);

  Serial.println(F("GPIO initialized"));

  // Initialize I2C with mutex protection
  Wire.begin();

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000))) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("WARNING: OLED init failed - continuing without display"));
      displayAvailable = false;
    } else {
      Serial.println(F("OLED initialized successfully"));
      displayAvailable = true;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println(F("ESP32 Multitool"));
      display.println(F("Initializing..."));
      display.display();
    }
    xSemaphoreGive(i2cMutex);
  }

  // Initialize rotary encoder
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(Pins::ROT_A, Pins::ROT_B);
  encoder.setCount(0);
  Serial.println(F("Encoder initialized"));

  // Initialize NeoPixel
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();
  Serial.println(F("NeoPixel initialized"));

  // Initialize PWM (LEDC) for 12V dimming (ESP32 core 3.x API)
  ledcAttach(Pins::PWM_MOSFET, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcWrite(Pins::PWM_MOSFET, 0);  // Start off
  Serial.println(F("PWM dimming initialized"));

  // Load web credentials
  loadWebCredentials();

  // Start WiFi task on Core 0
  TaskHandle_t wifiTaskHandle = nullptr;
  BaseType_t result = xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiTask",
    TaskConfig::WIFI_TASK_STACK,
    nullptr,
    TaskConfig::WIFI_TASK_PRIORITY,
    &wifiTaskHandle,
    TaskConfig::WIFI_TASK_CORE
  );

  if (result != pdPASS || wifiTaskHandle == nullptr) {
    Serial.println(F("FATAL: WiFi task creation failed!"));
    Serial.println(F("System will continue in offline mode"));
  } else {
    Serial.println(F("WiFi task created successfully on Core 0"));
  }

  // Monitor heap health
  size_t freeHeap = ESP.getFreeHeap();
  Serial.print(F("Free heap after setup: "));
  Serial.print(freeHeap);
  Serial.println(F(" bytes"));

  if (freeHeap < 50000) {
    Serial.println(F("WARNING: Low heap memory!"));
  }

  Serial.println(F("\nSetup complete - starting main loop\n"));
}

// --- MAIN LOOP (CORE 1) ---

void loop() {
  static unsigned long lastMemCheck = 0;

  // Feed watchdog
  esp_task_wdt_reset();

  // Read sensor with bounds checking
  int rawSensor = analogRead(Pins::SENSOR_IN);
  int constrainedSensor = constrain(rawSensor, 0, ADC_MAX_12BIT);

  // Update shared sensor value
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
    sharedState.sensorValue = constrainedSensor;
    xSemaphoreGive(stateMutex);
  }

  // Read relay state and update hardware
  static bool lastRelayState = false;
  bool currentRelayState = false;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
    currentRelayState = sharedState.relayState;
    xSemaphoreGive(stateMutex);
  }

  if (currentRelayState != lastRelayState) {
    digitalWrite(Pins::RELAY, currentRelayState ? HIGH : LOW);
    lastRelayState = currentRelayState;
    Serial.print(F("Relay: "));
    Serial.println(currentRelayState ? F("ON") : F("OFF"));
  }

  // Handle UI state machine
  switch (currentState) {
    case MENU: {
      long newPos = encoder.getCount() / 2;
      if (newPos < 0) {
        encoder.setCount((menuTotal - 1) * 2);
        newPos = menuTotal - 1;
      }
      if (newPos >= menuTotal) {
        encoder.setCount(0);
        newPos = 0;
      }
      menuSelection = (int)newPos;

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(F("ESP32 Multitool"));
        display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

        // Scrolling menu logic
        int visibleLines = 5;
        int startLine = menuSelection - 2;
        if (startLine < 0) startLine = 0;
        if (startLine > menuTotal - visibleLines) startLine = menuTotal - visibleLines;

        for (int i = 0; i < visibleLines && (startLine + i) < menuTotal; i++) {
          int itemIndex = startLine + i;
          display.setCursor(0, 15 + (i * 10));

          if (itemIndex == menuSelection) {
            display.print(F("> "));
          } else {
            display.print(F("  "));
          }
          display.println(menuItems[itemIndex]);
        }

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        currentState = (AppState)(menuSelection + 1);
        encoder.setCount(0);
      }
      break;
    }

    case APP_RELAY: {
      drawHeader("Relay Control");

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 30);
        display.setTextSize(2);
        display.println(currentRelayState ? F("ON") : F("OFF"));
        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
          sharedState.relayState = !sharedState.relayState;
          xSemaphoreGive(stateMutex);
        }
      }

      if (encoder.getCount() != 0) {
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_NEOPIXEL: {
      drawHeader("NeoPixel Ring");

      // Rainbow effect
      static long pixelHue = 0;
      static unsigned long lastPixUpdate = 0;

      if (millis() - lastPixUpdate > Timing::NEOPIXEL_ANIMATION_MS) {
        strip.rainbow(pixelHue);
        strip.show();
        pixelHue += 256;
        if (pixelHue >= 5 * 65536) pixelHue = 0;
        lastPixUpdate = millis();
      }

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 30);
        display.println(F("Rainbow cycling"));
        display.println(F("36 LEDs"));
        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        strip.clear();
        strip.show();
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_SENSOR: {
      drawHeader("Sensor Monitor");

      // Read calibrated voltage
      uint32_t millivolts = readCalibratedADC(Pins::SENSOR_IN);

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 15);
        display.setTextSize(1);
        display.print(F("Raw: "));
        display.println(constrainedSensor);

        display.setCursor(0, 28);
        display.setTextSize(2);
        display.print(millivolts / 1000.0, 2);
        display.println(F(" V"));

        // Bar graph
        display.setTextSize(1);
        display.drawRect(0, 50, 128, 10, SSD1306_WHITE);
        int barWidth = map(millivolts, 0, 3300, 0, 128);  // 0-3.3V range
        display.fillRect(0, 50, barWidth, 10, SSD1306_WHITE);

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_WIFI_STATUS: {
      drawHeader("WiFi Status");

      // Get WiFi info safely
      char currentIP[16];
      int currentClients;
      bool wifiOk;

      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
        strncpy(currentIP, sharedState.ipAddress, sizeof(currentIP));
        currentClients = sharedState.wifiClients;
        wifiOk = sharedState.wifiActive;
        xSemaphoreGive(stateMutex);
      } else {
        strcpy(currentIP, "Error");
        currentClients = 0;
        wifiOk = false;
      }

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 15);
        display.print(F("IP: "));
        display.println(currentIP);

        display.setCursor(0, 30);
        display.print(F("Clients: "));
        display.println(currentClients);

        display.setCursor(0, 45);
        display.print(F("Status: "));
        display.println(wifiOk ? F("ACTIVE") : F("OFFLINE"));

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_I2C: {
      drawHeader("I2C Scanner");
      static uint8_t foundDevices[128];
      static uint8_t deviceCount = 0;
      static bool scanComplete = false;
      static int scrollPosition = 0;

      // Perform scan once when entering this app
      if (!scanComplete) {
        deviceCount = 0;

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(500))) {
          // Scan I2C bus
          for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
              foundDevices[deviceCount++] = addr;
            }
          }
          xSemaphoreGive(i2cMutex);
        }
        scanComplete = true;
      }

      // Encoder controls scroll position
      long newPos = encoder.getCount() / 2;
      if (newPos < 0) {
        encoder.setCount(0);
        newPos = 0;
      }
      int maxScroll = deviceCount > 4 ? deviceCount - 4 : 0;
      if (newPos > maxScroll) {
        encoder.setCount(maxScroll * 2);
        newPos = maxScroll;
      }
      scrollPosition = (int)newPos;

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 15);
        display.print(F("Found: "));
        display.print(deviceCount);
        display.println(F(" devices"));

        if (deviceCount == 0) {
          display.setCursor(0, 30);
          display.println(F("No I2C devices"));
          display.println(F("detected!"));
        } else {
          // Display devices with scrolling
          for (int i = 0; i < 4 && (scrollPosition + i) < deviceCount; i++) {
            uint8_t addr = foundDevices[scrollPosition + i];
            display.setCursor(0, 30 + (i * 9));
            display.print(F("0x"));
            if (addr < 16) display.print(F("0"));
            display.print(addr, HEX);
            display.print(F(" "));
            const char* name = getI2CDeviceName(addr);
            // Truncate long names to fit
            char shortName[12];
            strncpy(shortName, name, 11);
            shortName[11] = '\0';
            display.println(shortName);
          }
        }

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        scanComplete = false;  // Reset for next time
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_SERVO: {
      drawHeader("Servo Control");
      static bool servoAttached = false;

      // Encoder controls angle (0-180)
      long newPos = encoder.getCount() / 2;
      if (newPos < 0) {
        encoder.setCount(0);
        newPos = 0;
      }
      if (newPos > 180) {
        encoder.setCount(180 * 2);
        newPos = 180;
      }
      int angle = (int)newPos;

      // Attach servo if not already attached
      if (!servoAttached) {
        myServo.attach(Pins::SERVO);
        servoAttached = true;
      }

      myServo.write(angle);

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 20);
        display.setTextSize(2);
        display.print(angle);
        display.println(F(" deg"));

        // Visual indicator
        display.setTextSize(1);
        display.setCursor(0, 45);
        display.print(F("0"));
        display.setCursor(110, 45);
        display.println(F("180"));
        display.drawRect(0, 55, 128, 8, SSD1306_WHITE);
        int barPos = map(angle, 0, 180, 0, 128);
        display.fillRect(barPos - 2, 55, 4, 8, SSD1306_WHITE);

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        myServo.detach();
        servoAttached = false;
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_PWM: {
      drawHeader("12V PWM Dimming");

      // Encoder controls brightness (0-255)
      long newPos = encoder.getCount() / 2;
      if (newPos < 0) {
        encoder.setCount(0);
        newPos = 0;
      }
      if (newPos > 255) {
        encoder.setCount(255 * 2);
        newPos = 255;
      }
      uint8_t brightness = (uint8_t)newPos;

      // Apply gamma correction and update PWM
      uint8_t corrected = gammaCorrect(brightness);
      ledcWrite(Pins::PWM_MOSFET, corrected);

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 20);
        display.setTextSize(2);
        display.print(brightness);
        display.println(F(" / 255"));

        display.setTextSize(1);
        display.setCursor(0, 40);
        uint8_t percent = (brightness * 100) / 255;
        display.print(F("Power: "));
        display.print(percent);
        display.println(F("%"));

        // Bar graph
        display.drawRect(0, 52, 128, 10, SSD1306_WHITE);
        int barWidth = map(brightness, 0, 255, 0, 128);
        display.fillRect(0, 52, barWidth, 10, SSD1306_WHITE);

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        ledcWrite(Pins::PWM_MOSFET, 0);  // Turn off
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_STEPPER: {
      drawHeader("Stepper Motor");

      // Encoder controls speed/direction
      long speed = encoder.getCount() / 4;  // -100 to +100
      if (speed < -100) {
        encoder.setCount(-100 * 4);
        speed = -100;
      }
      if (speed > 100) {
        encoder.setCount(100 * 4);
        speed = 100;
      }

      static unsigned long lastStepTime = 0;
      static int stepPosition = 0;

      // 28BYJ-48 half-step sequence (8 steps per cycle)
      static const uint8_t halfStepSeq[8][4] = {
        {1, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 1},
        {1, 0, 0, 1}
      };

      if (speed != 0) {
        unsigned long stepDelay = map(abs((int)speed), 1, 100, 20, 2);  // 2-20ms

        if (millis() - lastStepTime > stepDelay) {
          if (speed > 0) {
            stepPosition++;
            if (stepPosition >= 8) stepPosition = 0;
          } else {
            stepPosition--;
            if (stepPosition < 0) stepPosition = 7;
          }

          digitalWrite(Pins::STEP1, halfStepSeq[stepPosition][0]);
          digitalWrite(Pins::STEP2, halfStepSeq[stepPosition][1]);
          digitalWrite(Pins::STEP3, halfStepSeq[stepPosition][2]);
          digitalWrite(Pins::STEP4, halfStepSeq[stepPosition][3]);

          lastStepTime = millis();
        }
      } else {
        // Hold position or release (optional: release to save power)
        digitalWrite(Pins::STEP1, LOW);
        digitalWrite(Pins::STEP2, LOW);
        digitalWrite(Pins::STEP3, LOW);
        digitalWrite(Pins::STEP4, LOW);
      }

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 20);
        display.setTextSize(2);
        if (speed > 0) {
          display.print(F("CW "));
        } else if (speed < 0) {
          display.print(F("CCW "));
        } else {
          display.print(F("STOP"));
        }

        display.setTextSize(1);
        display.setCursor(0, 40);
        display.print(F("Speed: "));
        display.print(abs((int)speed));
        display.println(F("%"));

        display.setCursor(0, 50);
        display.print(F("Mode: Half-step"));

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        // Turn off all coils
        digitalWrite(Pins::STEP1, LOW);
        digitalWrite(Pins::STEP2, LOW);
        digitalWrite(Pins::STEP3, LOW);
        digitalWrite(Pins::STEP4, LOW);
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    case APP_I2S: {
      drawHeader("I2S Tone Gen");
      static bool dacEnabled = false;
      static unsigned long sampleIndex = 0;

      // Encoder controls frequency (100-4000 Hz)
      long newPos = encoder.getCount() / 2;
      if (newPos < TONE_FREQ_MIN) {
        encoder.setCount(TONE_FREQ_MIN * 2);
        newPos = TONE_FREQ_MIN;
      }
      if (newPos > TONE_FREQ_MAX) {
        encoder.setCount(TONE_FREQ_MAX * 2);
        newPos = TONE_FREQ_MAX;
      }
      uint16_t frequency = (uint16_t)newPos;

      // Initialize DAC on first entry
      if (!dacEnabled) {
        dac_output_enable(DAC_CHANNEL_1);  // GPIO25
        dacEnabled = true;
      }

      // Generate and output tone at approximate sample rate
      static unsigned long lastSampleTime = 0;
      unsigned long samplePeriod = 1000000 / DAC_SAMPLE_RATE;  // microseconds

      if (micros() - lastSampleTime >= samplePeriod) {
        uint8_t sample = generateSineSample(frequency, sampleIndex);
        dac_output_voltage(DAC_CHANNEL_1, sample);
        sampleIndex++;
        lastSampleTime = micros();

        // Prevent overflow
        if (sampleIndex > 1000000) sampleIndex = 0;
      }

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 20);
        display.setTextSize(2);
        display.print(frequency);
        display.println(F(" Hz"));

        display.setTextSize(1);
        display.setCursor(0, 40);
        display.print(F("DAC: GPIO25"));

        display.setCursor(0, 50);
        display.print(F("Range: "));
        display.print(TONE_FREQ_MIN);
        display.print(F("-"));
        display.print(TONE_FREQ_MAX);
        display.println(F("Hz"));

        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        // Disable DAC
        dac_output_disable(DAC_CHANNEL_1);
        dacEnabled = false;
        sampleIndex = 0;
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }

    // Default handler for other apps (placeholders)
    default: {
      drawHeader("Coming Soon");

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 25);
        display.println(F("Feature not"));
        display.println(F("implemented yet"));
        display.println();
        display.println(F("Press to exit"));
        display.display();
        xSemaphoreGive(i2cMutex);
      }

      if (buttonPressed()) {
        currentState = MENU;
        encoder.setCount(menuSelection * 2);
      }
      break;
    }
  }

  // Periodic heap monitoring (every 10 seconds)
  if (millis() - lastMemCheck > 10000) {
    size_t freeHeap = ESP.getFreeHeap();
    Serial.print(F("Free heap: "));
    Serial.print(freeHeap);
    Serial.println(F(" bytes"));

    if (freeHeap < 30000) {
      Serial.println(F("WARNING: Heap getting low!"));
    }

    lastMemCheck = millis();
  }

  // Yield to scheduler
  delay(10);
}
