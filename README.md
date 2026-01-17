# ESP32 Swiss Army Multitool

A production-ready ESP32-based hardware control system with WiFi capabilities, OLED display interface, and support for multiple peripherals. Designed for makers, hobbyists, and embedded systems enthusiasts.

## Features

- **WiFi Access Point** with secure web interface for remote control
- **OLED Display** (128x64 SSD1306) with rotary encoder navigation
- **Multiple Peripheral Support:**
  - Relay control (local and remote)
  - 36-LED NeoPixel ring with rainbow effects
  - Analog sensor monitoring
  - Servo motor control (placeholder)
  - Stepper motor control (placeholder)
  - I2S audio output (placeholder)
  - 12V PWM dimming (placeholder)
- **Dual-Core Architecture** utilizing ESP32's FreeRTOS
- **Thread-Safe Communication** between cores using mutexes
- **Memory-Optimized** with heap monitoring and fragmentation prevention
- **Security Features:**
  - HTTP Basic Authentication
  - CSRF protection via POST-only state changes
  - WiFi credential storage in NVS
  - WiFiManager for easy configuration

## Hardware Requirements

### Essential Components
- **ESP32 Development Board** (DevKit, WROOM-32, or similar)
- **SSD1306 OLED Display** (128x64 pixels, I2C, address 0x3C)
- **Rotary Encoder** with push button
- **Power Supply** (5V/1A minimum, 5V/2A recommended)

### Optional Components
- **36-LED NeoPixel Ring** (WS2812B)
- **Relay Module** (5V coil, optoisolator recommended)
- **Analog Sensor** (any sensor with 0-3.3V output)
- **Servo Motor** (3.3V or 5V compatible)
- **Stepper Motor** with driver board
- **12V MOSFET** for PWM dimming
- **I2S DAC Module** for audio output

## Wiring Diagram

### I2C (Display)
```
ESP32 SDA (GPIO21) → SSD1306 SDA
ESP32 SCL (GPIO22) → SSD1306 SCL
3.3V → SSD1306 VCC
GND → SSD1306 GND
```

### Rotary Encoder
```
ESP32 GPIO32 → Encoder A
ESP32 GPIO33 → Encoder B
ESP32 GPIO25 → Encoder Button (with pull-up)
GND → Encoder GND
```

### NeoPixel Ring
```
ESP32 GPIO15 → NeoPixel DIN
5V → NeoPixel 5V (external power recommended)
GND → NeoPixel GND
```

### Relay
```
ESP32 GPIO23 → Relay IN
5V → Relay VCC
GND → Relay GND
```

### Analog Sensor
```
Sensor Output → ESP32 GPIO34 (ADC1_CH6)
3.3V → Sensor VCC
GND → Sensor GND
```

**IMPORTANT:** GPIO34 is input-only. Do not use output pins for analog input.

## Software Requirements

### Arduino IDE

Install the following libraries via Arduino Library Manager:
- `Adafruit GFX Library` by Adafruit
- `Adafruit SSD1306` by Adafruit
- `ESP32Encoder` by Kevin Harrington
- `ESP32Servo` by Kevin Harrington
- `Adafruit NeoPixel` by Adafruit
- `WiFiManager` by tzapu

### PlatformIO

Use the included `platformio.ini` configuration:

```bash
git clone https://github.com/yourusername/esp32_swiss_army.git
cd esp32_swiss_army
pio run -t upload
pio device monitor
```

### Arduino CLI

```bash
# Install required libraries
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "ESP32Encoder"
arduino-cli lib install "ESP32Servo"
arduino-cli lib install "Adafruit NeoPixel"
arduino-cli lib install "WiFiManager"

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_swiss_army.ino

# Upload
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 esp32_swiss_army.ino

# Monitor serial
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Initial Setup

### First Boot

1. **Flash the firmware** to your ESP32
2. **Power cycle** the device
3. The device will create a WiFi access point named **`ESP32-Multitool-XXXX`** (XXXX = device ID)
4. **Connect** to this AP with password: `configure`
5. **Captive portal** should open automatically (if not, navigate to http://192.168.4.1)
6. **Configure WiFi** credentials for your network
7. Device will **reboot** and connect to your WiFi

### Web Interface

Once connected to WiFi:
1. Find the device's IP address (shown on OLED display or serial monitor)
2. Open browser to `http://[device-ip]`
3. Login with default credentials:
   - **Username:** `admin`
   - **Password:** `changeme`

**SECURITY WARNING:** Change the default password immediately!

To change the password, you'll need to modify the NVS storage or add a password change feature (see Contributing).

## Usage

### Hardware Controls

- **Rotate encoder** to navigate menus
- **Press encoder** to select menu item or toggle relay
- **Rotate in app** to return to menu

### Web Interface

- View real-time sensor readings
- Control relay remotely
- Monitor connected WiFi clients
- Check system heap memory

### Serial Monitor

Connect at 115200 baud to see:
- Boot messages
- WiFi connection status
- IP address
- Heap memory statistics
- Debug information

## Architecture

### Dual-Core Design

The ESP32 has two cores:
- **Core 0:** WiFi task (networking, web server)
- **Core 1:** Main loop (hardware control, display, sensors)

### Thread Safety

All shared variables are protected by FreeRTOS mutexes:
- `stateMutex` - Protects relay state, sensor values, WiFi status
- `i2cMutex` - Protects I2C bus (display operations)

### Memory Management

- **No String class** - Uses char arrays to prevent heap fragmentation
- **F() macro** - Stores strings in flash memory instead of RAM
- **Heap monitoring** - Tracks free memory every 10 seconds
- **Watchdog timer** - Automatic reset if system hangs (30s timeout)

## Customization

### Adding New Menu Items

1. Add enum value to `AppState` (line 136-148)
2. Increment `menuTotal` (line 152)
3. Add name to `menuItems[]` array (line 153-164)
4. Add case handler in main switch statement (line 612-807)

### Changing Pin Assignments

Edit the `Pins` namespace (line 44-65). Avoid strapping pins:
- GPIO0, GPIO2, GPIO5, GPIO12, GPIO15

### Adjusting WiFi Settings

Modify the `WiFiConfig` namespace (line 91-95):
- AP channel
- Max clients
- TX power

## Troubleshooting

### OLED Display Not Working

- Check I2C address (default 0x3C, some use 0x3D)
- Verify wiring (SDA=21, SCL=22)
- System will continue without display if init fails

### WiFi Not Connecting

- Hold reset during "config mode" message
- Connect to `ESP32-Multitool-XXXX` AP
- Clear saved credentials by holding encoder button during boot (requires modification)

### Brownout Detector Triggered

- Use adequate power supply (5V/2A minimum)
- Reduce NeoPixel brightness (line 81)
- Add bulk capacitors (100µF near ESP32, 1000µF on 5V rail)

### Heap Memory Low

- Check for memory leaks in custom code
- Reduce web interface complexity
- Monitor serial output for heap warnings

### Watchdog Timeout

- Check for blocking operations in loop()
- Ensure all delays are >= 10ms
- Verify mutexes are released properly

## Security Considerations

### Production Deployment

Before deploying to production:

1. **Change default password** from "changeme"
2. **Enable HTTPS** (requires ESP32_HTTPS_Server library)
3. **Enable NVS encryption** for credential storage
4. **Disable serial debugging** (comment out Serial.println)
5. **Enable Flash Encryption** + Secure Boot (ESP-IDF)

### Network Security

- Device creates open web interface on your network
- Use strong WiFi passwords
- Consider placing on isolated VLAN
- Monitor connected clients

## Performance

- **Boot time:** ~3-5 seconds
- **Web response:** <100ms
- **Display refresh:** 10ms
- **Sensor sampling:** 10ms (100 Hz)
- **Free heap:** ~180-200KB typical
- **WiFi task stack:** 8192 bytes
- **Main task stack:** ~4096 bytes

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Feature Requests

Open an issue with:
- Clear description of feature
- Use case/rationale
- Hardware requirements (if any)

### Bug Reports

Include:
- ESP32 board type
- Library versions
- Serial output
- Steps to reproduce

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- **Adafruit** - GFX and SSD1306 libraries
- **Kevin Harrington** - ESP32Encoder and ESP32Servo libraries
- **tzapu** - WiFiManager library
- **Espressif** - ESP32 platform and documentation

## Version History

### v2.0.0 (Production Ready)
- Added WiFiManager for easy configuration
- Implemented mutex protection for thread safety
- Replaced String with char arrays (memory optimization)
- Added HTTP authentication
- Added CSRF protection
- Improved error handling
- Added watchdog timer support
- Fixed GPIO2 strapping pin conflict
- Increased WiFi task stack size
- Added comprehensive documentation

### v1.0.0 (Initial Release)
- Basic functionality
- Hardcoded WiFi credentials
- No thread safety
- Memory leaks in web handler

## Support

For support, please:
1. Check the [Troubleshooting](#troubleshooting) section
2. Search existing [Issues](https://github.com/yourusername/esp32_swiss_army/issues)
3. Create a new issue with details

## Roadmap

- [ ] OTA firmware updates
- [ ] Web-based password change
- [ ] HTTPS/TLS support
- [ ] mDNS/Bonjour (esp32.local)
- [ ] MQTT support
- [ ] Home Assistant integration
- [ ] Implement placeholder apps (I2C scan, Servo, I2S, Stepper, GPS)
- [ ] ADC calibration
- [ ] Battery voltage monitoring
- [ ] Deep sleep mode
- [ ] Web-based file manager (SPIFFS/LittleFS)

---

**Made with ❤️ for the ESP32 community**
