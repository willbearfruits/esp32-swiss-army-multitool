# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based multitool device with the following features:
- OLED display (SSD1306, 128x64) with rotary encoder menu navigation
- WiFi Access Point ("S.H.I.T.") with web-based remote control
- Dual-core operation: WiFi server on Core 0, hardware control on Core 1
- Multiple hardware peripherals controlled through a menu system

## Hardware Configuration

### Pin Assignments
- **Display & Input**: I2C OLED (0x3C), Rotary encoder (pins 32, 33, switch 25)
- **Outputs**: Relay (23), Servo (13), NeoPixel ring (15), PWM/MOSFET (2), Stepper motor (18, 19, 4, 5)
- **I2S Audio**: BCLK (26), LRC (27), DOUT (14)
- **Sensor Input**: Analog pin 34

### Connected Peripherals
- 36-LED NeoPixel ring (brightness limited to 50 to prevent brownouts)
- Servo motor
- Relay module
- Stepper motor (4-wire)
- I2S DAC for audio output
- Analog sensor input
- 12V MOSFET dimmer circuit

## Architecture

### Dual-Core FreeRTOS Design
The sketch uses ESP32's dual cores:
- **Core 0**: Dedicated WiFi task (`wifiTask`) runs the access point and web server
  - WiFi AP configured on channel 6, max 4 connections, TX power 19.5dBm
  - Web server routes: `/` (dashboard), `/relay/on`, `/relay/off`
- **Core 1**: Main loop handles hardware I/O, display updates, and menu navigation
  - Reads sensor values, controls relay based on volatile shared state
  - Manages OLED display and user interface

### Shared State (Thread-Safe Volatiles)
Communication between cores uses volatile variables:
- `relayState` - relay on/off status
- `sensorValue` - analog sensor reading
- `wifiClients` - connected device count
- `wifiActive` - WiFi initialization flag
- `ipAddress` - AP IP address string

### Application State Machine
The menu system uses an enum-based state machine (`AppState`) with 10 applications:
1. Relay control
2. I2C scanner
3. Servo control
4. I2S tone generator
5. Stepper motor
6. GPS raw data
7. NeoPixel ring effects
8. Analog sensor display
9. 12V PWM dimming
10. WiFi status

Navigation: Rotate encoder to scroll, press to select. In any app, rotate encoder to return to menu.

## Development Commands

### Building & Uploading
This is an Arduino sketch for ESP32. Use Arduino IDE, PlatformIO, or Arduino CLI:

```bash
# Arduino CLI (if installed)
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_swiss_army.ino
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 esp32_swiss_army.ino
```

### Required Libraries
Install these via Arduino Library Manager or PlatformIO:
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `ESP32Encoder` (by Kevin Harrington)
- `ESP32Servo`
- `Adafruit NeoPixel`

Built-in ESP32 libraries (no install needed):
- `Wire.h`, `WiFi.h`, `WebServer.h`, `driver/i2s.h`

### Serial Monitoring
```bash
# Monitor serial output at 115200 baud
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
# Or use screen/minicom
screen /dev/ttyUSB0 115200
```

## WiFi Access Point

- **SSID**: `S.H.I.T.`
- **Password**: `SHITISIZERSCOM`
- **Default IP**: 192.168.4.1 (standard ESP32 AP IP)
- **Web Interface**: Green-on-black terminal aesthetic with relay control buttons

The web dashboard shows real-time sensor readings, relay status, and connected client count.

## Code Patterns & Conventions

### Display Rendering
Use `drawHeader(title)` helper for consistent screen layout. Always call `display.display()` after drawing operations.

### Button Debouncing
The `buttonPressed()` function includes 250ms debounce. Use this instead of raw `digitalRead(ROT_SW)`.

### Encoder Input
Encoder count is divided by 2 for menu navigation to reduce sensitivity. Reset to 0 when entering/exiting apps.

### NeoPixel Power Management
Brightness is capped at 50/255 to prevent brownout on 36-LED ring. Full brightness can draw excessive current.

### WebServer Response Pattern
All control endpoints (`/relay/on`, `/relay/off`) use HTTP 303 redirects back to root to refresh the dashboard.

## Common Modifications

### Adding New Menu Items
1. Add new enum value to `AppState`
2. Increment `menuTotal` constant
3. Add menu label to `menuItems[]` array
4. Add case handler in main `switch(currentState)` block

### Changing WiFi Credentials
Modify `ssid` and `password` constants (lines 29-31). Note: Password must be 8+ characters for WPA2.

### Adjusting Display Layout
Display is 128x64 pixels. Header uses rows 0-10, content starts at row 15. Text size 1 = 6x8 pixels per character.

## Important Notes

- The web server endpoint handlers modify volatile shared state, which is read/written by Core 1
- Watchdog timer requires `delay(10)` in WiFi task loop to prevent WDT crashes
- Some app cases (I2C, Servo, I2S, Stepper, GPS) show simplified placeholders - logic not fully implemented
- Sensor input is 12-bit ADC (0-4095 range)
