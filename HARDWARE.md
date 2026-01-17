# Hardware Setup Guide

Complete wiring reference for ESP32 Swiss Army Multitool.

## Table of Contents
- [Minimum Setup](#minimum-setup)
- [Full Setup](#full-setup)
- [Pin Reference](#pin-reference)
- [Power Requirements](#power-requirements)
- [PCB Design Recommendations](#pcb-design-recommendations)

## Minimum Setup

For basic functionality, you need:

### Required Components
1. ESP32 Development Board
2. SSD1306 OLED Display (128x64, I2C)
3. Rotary Encoder with button
4. USB cable for power/programming

### Wiring

```
ESP32          →    OLED Display (I2C)
------------------------
3.3V           →    VCC
GND            →    GND
GPIO21 (SDA)   →    SDA
GPIO22 (SCL)   →    SCL

ESP32          →    Rotary Encoder
------------------------
GPIO32         →    CLK (A)
GPIO33         →    DT (B)
GPIO25         →    SW (Button)
GND            →    GND
3.3V           →    + (if required)
```

## Full Setup

### Complete Bill of Materials

| Component | Specification | Qty | Notes |
|-----------|--------------|-----|-------|
| ESP32 DevKit | ESP32-WROOM-32 | 1 | Any ESP32 variant |
| OLED Display | SSD1306, 128x64, I2C | 1 | 0.96" or 1.3" |
| Rotary Encoder | KY-040 or similar | 1 | With push button |
| NeoPixel Ring | WS2812B, 36 LEDs | 1 | Or any count, update code |
| Relay Module | 5V, 10A | 1 | Optoisolator type |
| Power Supply | 5V, 2A | 1 | USB or barrel jack |
| Capacitor | 1000µF, 16V | 1 | For NeoPixel power |
| Capacitor | 100µF, 16V | 1 | Near ESP32 VIN |
| Resistor | 470Ω | 1 | For NeoPixel data line |
| Breadboard | Half size | 1 | Or custom PCB |
| Jumper Wires | Various | 20+ | 22 AWG recommended |

### Optional Components

| Component | Specification | Purpose |
|-----------|--------------|---------|
| Servo Motor | SG90 or similar | Position control |
| Stepper Motor | 28BYJ-48 + ULN2003 | Precise rotation |
| Analog Sensor | Any 0-3.3V output | Monitoring |
| MOSFET | IRLZ44N or similar | 12V PWM control |
| I2S DAC | MAX98357A | Audio output |
| GPS Module | NEO-6M or similar | Location tracking |

## Complete Wiring Diagram

```
                              ESP32 DevKit
                              ┌─────────────┐
                              │             │
         OLED SDA ────────────│21 (SDA)     │
         OLED SCL ────────────│22 (SCL)     │
                              │             │
       Encoder A ─────────────│32           │
       Encoder B ─────────────│33           │
    Encoder Button ───────────│25           │
                              │             │
     NeoPixel DIN ────────────│15           │
                              │             │
        Relay IN ─────────────│23           │
                              │             │
       Servo PWM ─────────────│13           │
                              │             │
   Analog Sensor ─────────────│34 (ADC1_6)  │
                              │             │
  MOSFET Gate (12V) ──────────│16           │
                              │             │
    Stepper IN1 ──────────────│18           │
    Stepper IN2 ──────────────│19           │
    Stepper IN3 ──────────────│4            │
    Stepper IN4 ──────────────│5            │
                              │             │
      I2S BCLK ───────────────│26           │
      I2S LRC ────────────────│27           │
      I2S DOUT ───────────────│14           │
                              │             │
         5V IN ───────────────│VIN          │
         GND ──────────────────│GND          │
         3.3V OUT ─────────────│3V3          │
                              └─────────────┘
```

## Pin Reference

### I2C Pins (Display, I2C Sensors)
- **GPIO21:** SDA (I2C Data)
- **GPIO22:** SCL (I2C Clock)

### Input Pins
- **GPIO32:** Rotary Encoder A
- **GPIO33:** Rotary Encoder B
- **GPIO25:** Rotary Encoder Button
- **GPIO34:** Analog Sensor (Input Only)

### Output Pins
- **GPIO23:** Relay Control
- **GPIO15:** NeoPixel Data
- **GPIO13:** Servo PWM
- **GPIO16:** 12V MOSFET Gate
- **GPIO18-19, 4-5:** Stepper Motor Control

### I2S Audio Pins
- **GPIO26:** I2S Bit Clock (BCLK)
- **GPIO27:** I2S Word Select (LRC)
- **GPIO14:** I2S Data Out (DOUT)

### Reserved/Strapping Pins (Not Used)
These pins affect boot mode and should not be used:
- **GPIO0:** Boot mode selection
- **GPIO2:** Boot mode (pulled down at boot)
- **GPIO5:** SDIO timing
- **GPIO12:** Flash voltage (DO NOT PULL UP)
- **GPIO15:** Debug output enable

## Power Requirements

### ESP32 Power Budget

| Component | Current Draw | Notes |
|-----------|-------------|-------|
| ESP32 (WiFi active) | 160-260mA | Peak 500mA during TX |
| ESP32 (WiFi idle) | 80-120mA | |
| OLED Display | 20-40mA | Depends on pixels lit |
| NeoPixel Ring (36 LEDs) | Up to 2.16A | 60mA per LED at full white |
| NeoPixel Ring (50% brightness) | ~700mA | Much safer |
| Relay Coil | 50-80mA | Only when energized |
| Servo Motor | 100-500mA | Depends on load |
| Stepper Motor | 200-400mA | Via ULN2003 driver |

### Recommended Power Supply

**Minimum:** 5V/1A USB power supply
**Recommended:** 5V/2A wall adapter or USB-C PD

### Power Distribution

```
5V Power Supply
    │
    ├──→ 100µF Capacitor ──→ ESP32 VIN
    │
    ├──→ 1000µF Capacitor ──→ NeoPixel 5V
    │
    └──→ Relay Module VCC
```

### Critical Power Notes

1. **NeoPixels:** MUST have separate 1000µF capacitor
2. **ESP32:** Add 100µF capacitor near VIN pin
3. **Brownout Prevention:** Use quality power supply
4. **Ground:** Common ground for all components
5. **Servo/Stepper:** Consider external 5V regulator for high-current loads

## OLED Display Variants

### 0.96" SSD1306 (128x64)
- **I2C Address:** 0x3C (most common) or 0x3D
- **Power:** 3.3V or 5V tolerant
- **Pins:** VCC, GND, SCL, SDA

### 1.3" SH1106 (128x64)
Similar to SSD1306 but different driver. Requires code change:
```cpp
// Replace in code
#include <Adafruit_SH1106.h>
Adafruit_SH1106 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
```

## NeoPixel Wiring Details

### Data Line Protection

```
ESP32 GPIO15 ──→ 470Ω Resistor ──→ NeoPixel DIN
```

The 470Ω resistor prevents voltage spikes that can damage the first LED.

### Power Wiring

```
         ┌─── NeoPixel 5V
         │
5V ──────┴─── 1000µF Capacitor ──── GND
         │
         └─── ESP32 VIN

ESP32 GND ──────────────────────────┴─── NeoPixel GND
```

### Important Notes

1. **Brightness Limit:** Code limits to 50/255 to prevent brownout
2. **Capacitor:** Place physically close to NeoPixel power pins
3. **Wire Gauge:** Use 20-22 AWG for power, 24-26 AWG for data
4. **Length:** Keep data wire < 6 inches if possible

## Relay Module Wiring

### Standard 5V Relay Module

```
ESP32 GPIO23 ──→ Relay IN
ESP32 GND ─────→ Relay GND
5V Supply ─────→ Relay VCC
```

### Relay Outputs

```
    NO (Normally Open) ──┐
    COM (Common) ────────┼──→ Load Control
    NC (Normally Closed)─┘
```

**Safety:**
- Use optoisolator-type relay modules
- Keep high voltage wiring separate
- Add flyback diode for inductive loads

## Analog Sensor Wiring

### Voltage Divider for 5V Sensors

ESP32 ADC maximum is 3.3V. For 5V sensors:

```
5V Sensor Output ──┬──→ 10kΩ ──→ ESP32 GPIO34
                   │
                   └──→ 20kΩ ──→ GND

Output Voltage = Sensor × (20kΩ / 30kΩ) = Sensor × 0.67
```

### Direct Connection (3.3V Sensors)

```
Sensor VCC ──→ ESP32 3.3V
Sensor OUT ──→ ESP32 GPIO34
Sensor GND ──→ ESP32 GND
```

## PCB Design Recommendations

### Layout Guidelines

1. **Decoupling Capacitors:** Place close to power pins
2. **Ground Plane:** Use continuous ground plane
3. **Power Traces:** Minimum 20 mil for 1A, 40 mil for 2A
4. **I2C Pullups:** 4.7kΩ to 3.3V (often on module)
5. **NeoPixel Data:** Keep trace short and away from noisy signals

### Connector Placement

```
[Power Input] ─── [ESP32] ─── [Display]
                     │
                     ├─── [Encoder]
                     │
                     ├─── [NeoPixel]
                     │
                     └─── [Relay/Outputs]
```

### Test Points

Add test points for:
- 5V rail
- 3.3V rail
- Ground
- Critical GPIO signals
- I2C bus (SDA, SCL)

## Breadboard Layout

```
        Power Rails
        + (5V)  - (GND)
        │       │
    ┌───┴───────┴───┐
    │  Breadboard    │
    │                │
    │   ESP32 ───────┼──→ Display
    │    │           │
    │    ├───────────┼──→ Encoder
    │    │           │
    │    ├───────────┼──→ Relay
    │    │           │
    │    └───────────┼──→ NeoPixel
    │                │
    └────────────────┘
```

Keep jumper wires organized and labeled.

## Troubleshooting Hardware

### Display Not Working

1. Check I2C address with scanner sketch
2. Verify 3.3V power
3. Swap SDA/SCL if needed
4. Test with separate sketch

### NeoPixels Not Lighting

1. Check 5V power supply capacity
2. Verify data pin connection
3. Add/check 470Ω resistor
4. Confirm ground connection
5. Test first LED separately

### Encoder Not Responding

1. Verify pull-up resistors (internal enabled in code)
2. Check connections
3. Swap A/B pins if direction inverted
4. Test continuity on button pin

### Relay Clicking But Not Switching

1. Check relay coil voltage (should be 5V)
2. Verify GPIO signal with multimeter
3. Test relay with direct 5V connection
4. Replace relay module if faulty

### ESP32 Keeps Resetting

1. **Brownout:** Increase power supply capacity
2. **Watchdog:** Check code for blocking operations
3. **Short Circuit:** Verify all wiring
4. **GPIO12 Issue:** Don't pull up during boot

## Safety Warnings

⚠️ **High Voltage:** If using relay for AC power, ensure proper insulation and enclosure

⚠️ **Current Limits:** ESP32 GPIO pins maximum 40mA total, 12mA per pin

⚠️ **ESD Protection:** Handle ESP32 with anti-static precautions

⚠️ **Power Polarity:** Double-check before powering on

⚠️ **Heat:** NeoPixels and power regulators can get hot at high current

## Additional Resources

- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [SSD1306 Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)

---

**Need help?** Open an issue on GitHub with photos of your setup!
