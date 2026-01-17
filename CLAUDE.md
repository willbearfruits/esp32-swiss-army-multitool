# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 Swiss Army Multitool v2.0 - A production-ready, open-source ESP32-based hardware control system with:
- OLED display (SSD1306, 128x64) with rotary encoder menu navigation
- WiFi connectivity via WiFiManager (captive portal configuration)
- Web-based remote control with HTTP authentication
- Dual-core FreeRTOS architecture for optimal performance
- Thread-safe communication using mutexes
- Memory-optimized design (no String class, F() macros throughout)

## Development Commands

### Building & Uploading

**Arduino CLI (recommended):**
```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_swiss_army.ino

# Upload (replace /dev/ttyUSB0 with your port)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 esp32_swiss_army.ino

# Monitor serial output
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**PlatformIO:**
```bash
pio run -t upload
pio device monitor
```

### Required Libraries

Install via Arduino Library Manager or PlatformIO:
- `Adafruit GFX Library` by Adafruit
- `Adafruit SSD1306` by Adafruit
- `ESP32Encoder` by Kevin Harrington
- `ESP32Servo` by Kevin Harrington
- `Adafruit NeoPixel` by Adafruit
- `WiFiManager` by tzapu

Built-in libraries (no install needed):
- `Wire.h`, `WiFi.h`, `WebServer.h`, `Preferences.h`, `esp_task_wdt.h`

### Library Installation via CLI
```bash
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306" \
  "ESP32Encoder" "ESP32Servo" "Adafruit NeoPixel" "WiFiManager"
```

## Architecture

### Dual-Core FreeRTOS Design

**Core 0 (PRO_CPU):**
- `wifiTask()` function running as FreeRTOS task
- Handles WiFi connection (via WiFiManager)
- Runs web server on port 80
- Priority: 2 (below lwIP at 18)
- Stack size: 8192 bytes

**Core 1 (APP_CPU):**
- Arduino `loop()` function
- Hardware I/O control (relay, sensor, NeoPixels)
- OLED display rendering
- Rotary encoder input handling
- UI state machine

### Thread-Safe Shared State

Communication between cores uses **mutexes** (not volatile variables):

**stateMutex** protects:
- `sharedState.relayState` - relay on/off status
- `sharedState.sensorValue` - analog sensor reading (0-4095)
- `sharedState.wifiClients` - connected device count
- `sharedState.wifiActive` - WiFi initialization flag
- `sharedState.ipAddress` - IP address as char[16]

**i2cMutex** protects:
- All I2C operations (display, potential I2C sensors)
- `display.display()` must hold mutex until completion

**Access pattern:**
```cpp
if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100))) {
  // Read or write shared state
  xSemaphoreGive(stateMutex);
}
```

### Memory Management

**Critical design decisions:**
- **No String class** - Uses `char[]` arrays to prevent heap fragmentation
- **F() macro** - All string literals stored in flash memory, not RAM
- **WiFiClient.println()** - Direct output instead of String concatenation
- **Heap monitoring** - Logs free heap every 10 seconds
- **Stack sizing** - WiFi task: 8192 bytes (increased from 4096)

### Security Features

- **WiFiManager** - No hardcoded credentials, captive portal configuration
- **HTTP Basic Auth** - Username/password on all web endpoints
- **CSRF Protection** - POST-only for state-changing operations
- **Input Validation** - `constrain()` on ADC readings
- **NVS Storage** - Credentials stored in Preferences (can be encrypted)

## Hardware Configuration

### Pin Assignments (Namespace: `Pins`)

**Input pins:**
- GPIO32: Rotary encoder A
- GPIO33: Rotary encoder B
- GPIO25: Rotary encoder button
- GPIO34: Analog sensor (input-only pin)

**Output pins (avoiding strapping pins):**
- GPIO23: Relay control
- GPIO13: Servo PWM
- GPIO15: NeoPixel data (36 LEDs)
- GPIO16: 12V MOSFET gate (changed from GPIO2 - strapping pin conflict)
- GPIO18,19,4,5: Stepper motor control

**I2C pins (fixed):**
- GPIO21: SDA
- GPIO22: SCL

**I2S pins (reserved):**
- GPIO26: BCLK
- GPIO27: LRC
- GPIO14: DOUT

### Configuration Constants

Organized in namespaces for clarity:
- `Pins::` - All pin assignments
- `Timing::` - Debounce, delays, watchdog timeout
- `TaskConfig::` - FreeRTOS task parameters
- `WiFiConfig::` - AP channel, max clients, TX power

### Power Considerations

- **NeoPixel brightness:** Limited to 50/255 (20%) to prevent brownout
- **36 LEDs at full white:** ~2.16A (60mA × 36)
- **36 LEDs at 50% brightness:** ~720mA
- **Recommended PSU:** 5V/2A minimum

## Application State Machine

**AppState enum** defines 10 menu items:
1. `APP_RELAY` - Toggle relay via button or web
2. `APP_I2C` - I2C scanner (placeholder)
3. `APP_SERVO` - Servo control (placeholder)
4. `APP_I2S` - Audio tone generator (placeholder)
5. `APP_STEPPER` - Stepper motor (placeholder)
6. `APP_GPS` - GPS data (placeholder)
7. `APP_NEOPIXEL` - Rainbow animation (implemented)
8. `APP_SENSOR` - Analog sensor with bar graph (implemented)
9. `APP_PWM` - 12V dimmer (placeholder)
10. `APP_WIFI_STATUS` - IP, clients, status (implemented)

**Navigation:**
- Rotate encoder: Scroll menu
- Press encoder: Select item / toggle relay
- Rotate in app: Return to menu

## Code Patterns & Best Practices

### Display Operations

All display operations must acquire `i2cMutex`:
```cpp
if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))) {
  display.clearDisplay();
  display.println("Text");
  display.display();  // Hold mutex until this completes!
  xSemaphoreGive(i2cMutex);
}
```

Use `drawHeader(title)` helper for consistent layout.

### Web Server Pattern

**All endpoints require authentication:**
```cpp
if (!server.authenticate(www_username, www_password)) {
  return server.requestAuthentication();
}
```

**State-changing endpoints use POST only:**
```cpp
if (server.method() != HTTP_POST) {
  server.send(405, "text/plain", "Method Not Allowed");
  return;
}
```

**Use WiFiClient for HTML generation (not String):**
```cpp
WiFiClient client = server.client();
client.println(F("HTTP/1.1 200 OK"));
client.println(F("Content-Type: text/html"));
client.println();
client.println(F("<html>..."));
```

### Watchdog Timer

**ESP32 Core 3.x API:**
```cpp
esp_task_wdt_config_t wdt_config = {
  .timeout_ms = Timing::WATCHDOG_TIMEOUT_MS,
  .idle_core_mask = 0,
  .trigger_panic = true
};
esp_task_wdt_init(&wdt_config);
esp_task_wdt_add(NULL);  // Add task to monitoring

// In loop/task:
esp_task_wdt_reset();  // Feed watchdog
```

### FreeRTOS Delays

Always use `vTaskDelay(pdMS_TO_TICKS(ms))` with minimum 10ms:
```cpp
vTaskDelay(pdMS_TO_TICKS(10));  // Never less than 10ms
```

## Common Modifications

### Adding New Menu Items

1. Add enum value to `AppState` (line 136-148)
2. Increment `menuTotal` (line 152)
3. Add label to `menuItems[]` (line 153-164)
4. Add case handler in `switch(currentState)` (line 612-807)

Example:
```cpp
// 1. Add to enum
enum AppState {
  // ... existing items
  APP_NEW_FEATURE  // Add here
};

// 2. Update total
const int menuTotal = 11;  // Increment

// 3. Add label
const char* menuItems[] = {
  // ... existing items
  "New Feature"  // Add here
};

// 4. Add case handler
case APP_NEW_FEATURE: {
  drawHeader("New Feature");
  // Implementation
  if (buttonPressed()) {
    currentState = MENU;
    encoder.setCount(menuSelection * 2);
  }
  break;
}
```

### Changing Pin Assignments

Edit `Pins` namespace (line 44-65). **Avoid ESP32 strapping pins:**
- GPIO0, GPIO2, GPIO5, GPIO12, GPIO15

### Modifying WiFi Settings

Edit `WiFiConfig` namespace (line 91-95):
- `AP_CHANNEL` - WiFi channel (1-13)
- `MAX_CLIENTS` - Max concurrent connections
- `TX_POWER` - Transmission power

### Adjusting Timing Constants

Edit `Timing` namespace (line 68-74):
- `DEBOUNCE_MS` - Button debounce delay
- `WATCHDOG_TIMEOUT_MS` - WDT timeout (30 seconds)
- `NEOPIXEL_ANIMATION_MS` - Rainbow effect speed

## WiFi Configuration

### First Boot

1. Device creates AP: `ESP32-Multitool-XXXX` (XXXX = MAC suffix)
2. Password: `configure`
3. Captive portal opens automatically
4. Configure WiFi credentials
5. Device reboots and connects

### Web Interface Access

Once connected:
- IP shown on OLED display and serial monitor
- Access: `http://[IP-ADDRESS]`
- **Default login:** admin / changeme
- ⚠️ **IMPORTANT:** Change default password!

### Credential Storage

Credentials stored in NVS via `Preferences` library:
- WiFi credentials: Managed by WiFiManager
- Web auth: `preferences.begin("auth")`

To change web password, modify NVS or implement password change feature.

## Debugging & Troubleshooting

### Serial Output (115200 baud)

Key messages:
- `Mutexes created successfully` - Thread safety initialized
- `OLED initialized successfully` - Display ready
- `WiFi task created successfully on Core 0` - Dual-core running
- `Free heap: XXXXX bytes` - Memory health (logged every 10s)
- `WARNING: Heap getting low!` - Heap < 30KB

### Common Issues

**Watchdog timeout:**
- Check for blocking operations in tasks
- Ensure `esp_task_wdt_reset()` called regularly
- Verify delays >= 10ms

**OLED init failed:**
- Check I2C address (0x3C or 0x3D)
- Verify wiring (SDA=21, SCL=22)
- System continues without display (graceful degradation)

**Brownout detector:**
- Reduce NeoPixel brightness
- Use adequate power supply (5V/2A)
- Add bulk capacitors

**Heap fragmentation:**
- Avoid String class
- Monitor heap with serial output
- Check for memory leaks in custom code

## Production Deployment Checklist

Before deploying:
- [ ] Change default web password from "changeme"
- [ ] Test with actual hardware loads
- [ ] Monitor heap usage over extended period (1+ hour)
- [ ] Verify watchdog doesn't trigger under load
- [ ] Test WiFi reconnection after network loss
- [ ] Confirm OLED failure doesn't crash system
- [ ] Validate all mutex operations have timeouts
- [ ] Check GPIO pin assignments match hardware

## Version History

**v2.0.0 (Production Ready):**
- Added WiFiManager for configuration
- Implemented mutex protection (thread-safe)
- Replaced String with char arrays
- Added HTTP authentication
- Fixed GPIO2 strapping pin conflict
- Increased WiFi task stack (8192 bytes)
- Updated watchdog timer for ESP32 core 3.x
- Comprehensive documentation

**v1.0.0 (Initial):**
- Basic functionality
- Hardcoded credentials (security issue)
- No thread safety
- Memory leaks in web handler

## File Structure

```
esp32_swiss_army/
├── esp32_swiss_army.ino        # Main sketch (826 lines)
├── CLAUDE.md                    # This file
├── README.md                    # User documentation
├── HARDWARE.md                  # Wiring diagrams
├── CONTRIBUTING.md              # Contributor guidelines
├── LICENSE                      # MIT License
├── platformio.ini               # PlatformIO config
├── .gitignore                   # Git ignore rules
└── examples/
    └── simple_relay_control/    # Minimal example
```

## Important Notes

- **ESP32 core version:** Tested with 3.3.5 (uses new watchdog API)
- **Compilation size:** ~1.1MB (84% of flash)
- **RAM usage:** ~49KB static, 260KB free heap typical
- **WiFi task priority:** 2 (below lwIP at 18, above loop at 1)
- **Thread safety:** All cross-core access protected by mutexes
- **Placeholder apps:** I2C scan, Servo, I2S, Stepper, GPS not implemented
- **ADC resolution:** 12-bit (0-4095 range)
