# ESP32 Multitool - Modern Web Interface Integration Guide

## Overview

This guide explains how to integrate the new modern web interface into your existing ESP32 Multitool sketch.

## Features

### Dashboard Page (`/`)
- **Real-time System Status**: IP address, connected clients, heap memory, uptime, WiFi signal
- **Relay Control**: Toggle buttons with visual state indicators
- **Sensor Monitoring**: Live ADC values with animated chart (50-point history)
- **PWM Dimmer**: Slider control for 12V dimming (0-100%) with real-time feedback
- **Servo Control**: Angle selector (0-180°) with visual position indicator
- **I2C Scanner**: Button to scan and display detected I2C devices

### Settings Page (`/settings`)
- **Password Management**: Change web and OTA passwords
- **MQTT Configuration**: Broker, port, client ID, credentials
- **Network Information**: Display SSID, IP, MAC address
- **System Actions**: WiFi reset, device reboot

### OTA Update Page (`/ota`)
- **Firmware Upload**: Drag-and-drop .bin file upload
- **Progress Tracking**: Real-time upload progress with percentage
- **Firmware Info**: Current version, build date, sketch size, free space

## Design Highlights

### Cyberpunk/Matrix Aesthetic
- Black background (#000) with Matrix green accent (#0f0)
- Monospace font (Courier New) for terminal vibe
- Glowing text effects and animations
- Card-based layout with hover effects
- Responsive design (mobile-friendly)

### Performance Optimizations
- **Inline CSS/JavaScript**: No external files, all embedded in HTML
- **Compressed HTML**: ~9KB per page (fits in ESP32 memory)
- **Minimal JavaScript**: Vanilla JS, no heavy frameworks
- **Efficient Updates**: AJAX polling every 1 second
- **Canvas Charting**: Lightweight sensor visualization

### Accessibility
- High contrast (green on black)
- Clear visual hierarchy
- Touch-friendly buttons (large targets)
- Keyboard navigation support
- Screen reader compatible semantic HTML

## Installation Instructions

### Step 1: Add Required Libraries

Add to your `platformio.ini` or Arduino IDE Library Manager:

```ini
lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    madhephaestus/ESP32Encoder
    madhephaestus/ESP32Servo
    adafruit/Adafruit NeoPixel
    WiFiManager
    PubSubClient
    bblanchon/ArduinoJson @ ^6.21.0  ; <-- ADD THIS
```

### Step 2: Include Headers in Your Sketch

Add these includes at the top of `esp32_swiss_army.ino` (after existing includes):

```cpp
#include <ArduinoJson.h>
#include "web_interface_dashboard.h"
#include "web_interface_settings.h"
#include "web_interface_ota.h"
#include "web_api_handlers.h"
```

### Step 3: Replace Web Server Handlers

**FIND** this section in your code (around line 770-780):

```cpp
// Setup web server routes
server.on("/", HTTP_GET, handleRoot);
server.on("/relay/on", HTTP_POST, handleRelayOn);
server.on("/relay/off", HTTP_POST, handleRelayOff);
server.on("/password", HTTP_GET, handlePasswordForm);
server.on("/password/update", HTTP_POST, handlePasswordUpdate);
server.onNotFound(handleNotFound);

server.begin();
```

**REPLACE** with:

```cpp
// Setup web server routes - NEW MODERN INTERFACE
server.on("/", HTTP_GET, []() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  server.send_P(200, "text/html", DASHBOARD_HTML);
});

server.on("/settings", HTTP_GET, []() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  server.send_P(200, "text/html", SETTINGS_HTML);
});

server.on("/ota", HTTP_GET, []() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  server.send_P(200, "text/html", OTA_HTML);
});

server.onNotFound([]() {
  server.send(404, "text/plain", "Not Found");
});

// Register all API handlers
registerAPIHandlers();

server.begin();
```

### Step 4: Remove Old Handler Functions (OPTIONAL)

You can optionally remove these old functions to save space (they're no longer used):

- `handleRoot()`
- `handleRelayOn()`
- `handleRelayOff()`
- `handlePasswordForm()`
- `handlePasswordUpdate()`

**WARNING**: Keep `loadWebCredentials()` and authentication variables - they're still used!

### Step 5: Upload and Test

1. Compile the sketch
2. Upload to your ESP32
3. Open browser to your ESP32's IP address
4. Log in with your credentials (default: admin/changeme)
5. Test all features:
   - Toggle relay
   - Adjust PWM slider
   - Move servo slider
   - Scan I2C bus
   - Check sensor graph updates

## File Structure

```
esp32_swiss_army/
├── esp32_swiss_army.ino          # Main sketch (modify as above)
├── web_interface_dashboard.h      # Dashboard HTML
├── web_interface_settings.h       # Settings HTML
├── web_interface_ota.h            # OTA Update HTML
├── web_api_handlers.h             # API endpoint handlers
└── WEB_INTERFACE_INTEGRATION.md   # This file
```

## Memory Usage

| Component | Size | Notes |
|-----------|------|-------|
| Dashboard HTML | ~8.5 KB | Compressed, inline CSS/JS |
| Settings HTML | ~6.2 KB | Form-based interface |
| OTA HTML | ~6.8 KB | Upload interface |
| API Handlers | ~4 KB | Code overhead |
| **Total** | **~25 KB** | Fits comfortably in ESP32 |

**Heap Impact**: ~15-20 KB during runtime (JSON buffers, string operations)

**Flash Impact**: ~25 KB program storage (stored in PROGMEM)

## API Endpoints Reference

### GET /api/status
Returns system status including relay state, sensor value, WiFi info, heap memory.

**Response:**
```json
{
  "relay": true,
  "sensor": 2048,
  "clients": 1,
  "ip": "192.168.1.100",
  "heap": 234567,
  "uptime": 12345678,
  "rssi": -45,
  "pwm": 50,
  "servo": 90
}
```

### POST /api/relay
Control relay state.

**Request:**
```json
{"state": true}
```

### POST /api/pwm
Set PWM dimmer value (0-100%).

**Request:**
```json
{"value": 75}
```

### POST /api/servo
Set servo angle (0-180°).

**Request:**
```json
{"angle": 135}
```

### GET /api/i2c/scan
Scan I2C bus for devices.

**Response:**
```json
{
  "devices": [
    {"addr": 60, "name": "SSD1306 OLED"},
    {"addr": 104, "name": "MPU6050/DS3231"}
  ]
}
```

### POST /api/password
Update web and OTA passwords.

**Request:**
```json
{
  "current": "oldpass",
  "newpass": "newpass123",
  "otapass": "otapass123"
}
```

### POST /api/mqtt
Configure MQTT settings.

**Request:**
```json
{
  "server": "broker.hivemq.com",
  "port": 1883,
  "client": "ESP32_Multitool",
  "user": "username",
  "pass": "password"
}
```

### GET /api/network
Get network information.

### GET /api/firmware
Get firmware information.

### POST /api/wifi/reset
Reset WiFi settings (triggers reboot).

### POST /api/reboot
Reboot device.

### POST /update
Upload firmware .bin file (multipart/form-data).

## Troubleshooting

### Compilation Errors

**Error: ArduinoJson not found**
- Install ArduinoJson library (v6.21+)
- Add to `platformio.ini`: `bblanchon/ArduinoJson @ ^6.21.0`

**Error: Undefined reference to 'gammaCorrect'**
- Make sure `gammaCorrect()` function exists in your main sketch
- It's already in the provided code around line 255

**Error: Out of memory**
- Reduce `maxDataPoints` in JavaScript (line 442)
- Remove unused features
- Increase partition size in `platformio.ini`

### Runtime Issues

**Dashboard not updating**
- Check browser console for errors
- Verify `/api/status` returns valid JSON
- Ensure WiFi connection is stable

**Slider not working**
- Check `/api/pwm` and `/api/servo` endpoints
- Verify servo is attached to correct pin
- Check PWM channel configuration

**I2C scan fails**
- Ensure `i2cMutex` is created in setup()
- Check I2C devices are connected
- Verify pull-up resistors on SDA/SCL

**OTA upload fails**
- File must be .bin format
- Check available flash space
- Ensure stable WiFi connection
- Verify OTA password

## Customization

### Change Color Scheme

Edit the `:root` CSS variables in each HTML file:

```css
:root {
  --bg-primary: #000;        /* Background */
  --accent: #0f0;            /* Matrix green */
  --text: #0f0;              /* Text color */
  --danger: #f00;            /* Danger/error */
}
```

**Popular alternatives:**
- **Neon Blue**: `--accent: #0ff;`
- **Amber Terminal**: `--accent: #ffb000;`
- **Red Alert**: `--accent: #f00;`
- **Purple Cyberpunk**: `--accent: #c0f;`

### Adjust Update Frequency

In dashboard JavaScript, change polling interval:

```javascript
setInterval(updateDashboard, 1000);  // 1000ms = 1 second
```

**Recommendations:**
- Fast updates (500ms): High CPU/network usage
- Moderate (1000ms): Default, balanced
- Slow (2000ms): Saves resources

### Add Custom Sensors

1. Add data to `handleAPIStatus()` in `web_api_handlers.h`
2. Add HTML card to `DASHBOARD_HTML`
3. Add JavaScript update code to `updateDashboard()`

### Modify Chart Appearance

Edit chart drawing in dashboard JavaScript:

```javascript
ctx.strokeStyle = '#0f0';  // Line color
ctx.lineWidth = 2;         // Line thickness
let maxDataPoints = 50;    // History length
```

## Security Considerations

1. **HTTPS**: This interface uses HTTP. For production, use HTTPS with ESP32's SSL support
2. **Password Strength**: Enforce minimum 8 characters (already implemented)
3. **CSRF Protection**: API uses JSON POST, not URL parameters
4. **Input Validation**: All inputs are validated and constrained
5. **Authentication**: HTTP Basic Auth on all endpoints

## Performance Tips

1. **Reduce Update Frequency**: Increase `setInterval` delay to 2000ms
2. **Limit Chart Data**: Reduce `maxDataPoints` to 25-30
3. **Disable Features**: Comment out unused API handlers
4. **Optimize JSON**: Use `StaticJsonDocument` for fixed sizes
5. **GZIP Compression**: Enable server.enableGzip() if supported

## Browser Compatibility

| Browser | Version | Status |
|---------|---------|--------|
| Chrome | 80+ | ✓ Full support |
| Firefox | 75+ | ✓ Full support |
| Safari | 13+ | ✓ Full support |
| Edge | 80+ | ✓ Full support |
| Mobile Safari | iOS 13+ | ✓ Full support |
| Chrome Mobile | Latest | ✓ Full support |

**Note**: Internet Explorer is not supported (uses modern ES6 features).

## Future Enhancements

Potential additions (not implemented):

- WebSocket support for real-time updates (no polling)
- Multi-language support (i18n)
- Dark/light theme toggle
- Exportable sensor data (CSV download)
- Custom dashboard widgets
- Historical data logging
- HTTPS/TLS encryption
- User roles and permissions

## Support

For issues or questions:

1. Check existing code comments
2. Review this documentation
3. Test with browser console open
4. Check serial monitor for errors
5. Verify hardware connections

## License

MIT License - Same as ESP32 Multitool main project

---

**Version**: 1.0.0
**Last Updated**: 2025-01-17
**Compatible with**: ESP32 Multitool v2.0.0
