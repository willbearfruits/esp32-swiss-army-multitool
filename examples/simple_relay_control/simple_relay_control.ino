/*
 * Simple Relay Control Example
 *
 * Demonstrates thread-safe relay control using WiFi and local button
 * Minimal example extracted from ESP32 Swiss Army Multitool
 *
 * Hardware:
 * - ESP32 board
 * - Relay module on GPIO23
 * - Button on GPIO25 (with internal pull-up)
 *
 * Features:
 * - Dual-core operation (WiFi on Core 0, control on Core 1)
 * - Mutex-protected shared state
 * - Web interface with authentication
 * - Local button control
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

// Pin definitions
const int PIN_RELAY = 23;
const int PIN_BUTTON = 25;

// Web server
WebServer server(80);
WiFiManager wifiManager;

// Thread-safe shared state
SemaphoreHandle_t stateMutex;
struct {
  bool relayState;
  char ipAddress[16];
} sharedState;

// Credentials
const char* www_username = "admin";
const char* www_password = "changeme";

// Button debouncing
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 250;

// ============= Web Handlers =============

void handleRoot() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  bool currentRelay;
  char currentIP[16];

  // Read shared state safely
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    currentRelay = sharedState.relayState;
    strncpy(currentIP, sharedState.ipAddress, sizeof(currentIP));
    xSemaphoreGive(stateMutex);
  } else {
    server.send(503, "text/plain", "Service unavailable");
    return;
  }

  // Build HTML response
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE html><html><head><title>Relay Control</title>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
  client.println("<style>");
  client.println("body{font-family:Arial;text-align:center;padding:20px;background:#f0f0f0;}");
  client.println("h1{color:#333;}");
  client.println(".status{font-size:2em;margin:20px;padding:20px;background:white;border-radius:10px;}");
  client.println("button{padding:15px 30px;font-size:1.2em;margin:10px;cursor:pointer;border:none;border-radius:5px;}");
  client.println(".on{background:#4CAF50;color:white;}");
  client.println(".off{background:#f44336;color:white;}");
  client.println("</style></head><body>");
  client.println("<h1>Relay Control</h1>");
  client.print("<div class='status'>Status: <b>");
  client.print(currentRelay ? "ON" : "OFF");
  client.println("</b></div>");
  client.println("<form method='POST' action='/relay/on'>");
  client.println("<button class='on' type='submit'>Turn ON</button>");
  client.println("</form>");
  client.println("<form method='POST' action='/relay/off'>");
  client.println("<button class='off' type='submit'>Turn OFF</button>");
  client.println("</form>");
  client.print("<p>IP: ");
  client.print(currentIP);
  client.println("</p>");
  client.println("</body></html>");
  client.stop();
}

void handleRelayOn() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sharedState.relayState = true;
    xSemaphoreGive(stateMutex);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRelayOff() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }

  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
    sharedState.relayState = false;
    xSemaphoreGive(stateMutex);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// ============= WiFi Task (Core 0) =============

void wifiTask(void* parameter) {
  Serial.println("WiFi task starting...");

  // Auto-connect or start config portal
  char apName[32];
  snprintf(apName, sizeof(apName), "ESP32-Relay-%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  if (!wifiManager.autoConnect(apName, "configure")) {
    Serial.println("Failed to connect - resetting");
    delay(3000);
    ESP.restart();
  }

  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // Update shared IP
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(1000))) {
    IPAddress ip = WiFi.localIP();
    snprintf(sharedState.ipAddress, sizeof(sharedState.ipAddress),
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    xSemaphoreGive(stateMutex);
  }

  // Setup web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/relay/on", HTTP_POST, handleRelayOn);
  server.on("/relay/off", HTTP_POST, handleRelayOff);
  server.begin();

  Serial.println("Web server started");

  // Main WiFi loop
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============= Setup & Loop =============

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== Simple Relay Control Example ===\n");

  // Create mutex
  stateMutex = xSemaphoreCreateMutex();
  if (!stateMutex) {
    Serial.println("FATAL: Failed to create mutex!");
    while (1) delay(1000);
  }

  // Initialize state
  sharedState.relayState = false;
  strcpy(sharedState.ipAddress, "0.0.0.0");

  // Setup GPIO
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial.println("GPIO initialized");

  // Start WiFi task on Core 0
  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiTask",
    8192,
    NULL,
    1,
    NULL,
    0  // Core 0
  );

  Serial.println("Setup complete\n");
}

void loop() {
  // Check button (with debouncing)
  if (digitalRead(PIN_BUTTON) == LOW) {
    if (millis() - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = millis();

      // Toggle relay
      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
        sharedState.relayState = !sharedState.relayState;
        Serial.print("Button toggle -> ");
        Serial.println(sharedState.relayState ? "ON" : "OFF");
        xSemaphoreGive(stateMutex);
      }
    }
  }

  // Update hardware based on shared state
  static bool lastRelayState = false;
  bool currentRelayState;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))) {
    currentRelayState = sharedState.relayState;
    xSemaphoreGive(stateMutex);
  }

  if (currentRelayState != lastRelayState) {
    digitalWrite(PIN_RELAY, currentRelayState ? HIGH : LOW);
    lastRelayState = currentRelayState;
  }

  delay(10);
}
