# Simple Relay Control Example

Minimal example demonstrating thread-safe relay control with WiFi interface.

## What This Example Shows

- Dual-core FreeRTOS operation
- Mutex-protected shared state
- WiFi configuration via captive portal
- Web interface with authentication
- Local button control with debouncing
- Proper HTML generation without String class

## Hardware Needed

- ESP32 development board
- 5V relay module
- Push button
- Jumper wires

## Wiring

```
ESP32 GPIO23 → Relay IN
ESP32 GPIO25 → Button (other side to GND)
5V → Relay VCC
GND → Relay GND, Button GND
```

## Installation

1. Install required libraries:
   - WiFiManager (by tzapu)

2. Upload sketch to ESP32

3. On first boot:
   - Connect to WiFi AP "ESP32-Relay-XXXX"
   - Password: "configure"
   - Configure your WiFi credentials

4. Access web interface:
   - Find IP address in serial monitor
   - Open browser to http://[IP-ADDRESS]
   - Login: admin / changeme

## Features Demonstrated

### Thread Safety
- Mutex protects `relayState` variable
- Safe access from both cores
- No race conditions

### WiFi Manager
- Auto-connect to saved network
- Captive portal for configuration
- No hardcoded credentials

### Web Interface
- HTTP Basic Authentication
- POST-only for state changes (CSRF protection)
- Memory-efficient HTML generation

### Hardware Control
- Physical button with debouncing
- Relay state synchronization
- Serial logging

## Code Structure

```cpp
Core 0 (wifiTask)          Core 1 (loop)
├─ WiFi management         ├─ Button reading
├─ Web server             ├─ Hardware control
└─ State updates          └─ State updates
        │                         │
        └─── Mutex ───────────────┘
           (sharedState)
```

## Extending This Example

Add to this example:
- Timer-based relay control
- Multiple relays
- Scheduling
- MQTT integration
- Status LED indicators

## Learning Points

1. **Dual-Core:** Network on Core 0, hardware on Core 1
2. **Mutexes:** Always check return value of `xSemaphoreTake`
3. **Timeouts:** Use `pdMS_TO_TICKS(100)` not `portMAX_DELAY`
4. **Memory:** Use `WiFiClient.println()` not String concatenation
5. **Security:** Authenticate all web endpoints

## Troubleshooting

**Relay doesn't respond:**
- Check GPIO23 connection
- Verify relay module voltage (5V)
- Test with direct 5V to relay IN

**Can't connect to WiFi:**
- Reset and wait for AP mode
- Check AP name in serial monitor
- Verify password is "configure"

**Web interface won't load:**
- Check IP address in serial output
- Verify same network as computer
- Clear browser cache

## Next Steps

Try the full ESP32 Swiss Army Multitool for:
- OLED display with menu system
- Multiple peripherals
- Sensor monitoring
- Advanced features

## License

MIT License - Free to use and modify
