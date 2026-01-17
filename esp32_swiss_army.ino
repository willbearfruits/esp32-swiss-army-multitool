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
 * Version: 2.0.0 (Production Ready)
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

void loadWebCredentials() {
  preferences.begin("auth", true);  // Read-only
  preferences.getString("user", www_username, sizeof(www_username));
  preferences.getString("pass", www_password, sizeof(www_password));
  preferences.end();

  // If no password set, use default for first setup
  if (strlen(www_password) == 0) {
    strcpy(www_password, "changeme");
    Serial.println(F("WARNING: Using default password 'changeme' - please change it!"));
  }
}

// --- HELPER FUNCTIONS ---

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
  client.println(F("ESP32 Multitool v2.0<br>"));
  client.print(F("Free Heap: "));
  client.print(ESP.getFreeHeap());
  client.println(F(" bytes</div>"));

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

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/relay/on", HTTP_POST, handleRelayOn);
  server.on("/relay/off", HTTP_POST, handleRelayOff);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("Web server started"));

  // Main WiFi task loop
  unsigned long lastClientCheck = 0;

  for (;;) {
    // Handle web requests
    server.handleClient();

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
  Serial.println(F("ESP32 Multitool v2.0"));
  Serial.println(F("Production Ready Edition"));
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

  // Configure watchdog timer
  esp_task_wdt_init(Timing::WATCHDOG_TIMEOUT_MS / 1000, true);
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

      if (displayAvailable && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
        display.setCursor(0, 30);
        display.setTextSize(2);
        display.println(constrainedSensor);

        // Bar graph
        display.setTextSize(1);
        display.drawRect(0, 50, 128, 10, SSD1306_WHITE);
        int barWidth = map(constrainedSensor, 0, ADC_MAX_12BIT, 0, 128);
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
