# ESP32 RC Helicopter Controller - Setup & Wiring Guide

## Project Overview

This is a custom RC helicopter remote control system built with ESP32 WROOM32. It provides wireless control via a web-based interface accessible from a smartphone or computer.

**Key Features:**
- WiFi hotspot (AP mode) - no internet required
- Real-time motor control via web browser
- PWM signal generation for smooth motor control
- H-Bridge control for bidirectional tail rotor
- Trim adjustments for stability
- Persistent trim settings (saved to ESP32 flash)
- **Autoland Safety System** - automatic emergency descent on WiFi loss

---

## Hardware Requirements

### Main Components:
- **ESP32 WROOM32** (38-pin DevKit)
- **2x MOSFET** (e.g., IRF540N or similar) - for main rotor control
- **1x H-Bridge Module** (e.g., L298N) - for tail rotor control
- **2x DC Motors** - main rotors (opposite rotation)
- **1x DC Motor** - tail rotor
- **Power Supply** (suitable for your motors, typically 5-12V)
- **Wires & Resistors** (10k Ohm for gate pull-down)

### Optional:
- Capacitors (100µF) for power stabilization
- LED + 1k resistor for status indication
- Voltage regulator if needed

---

## Pinout Diagram

```
ESP32 WROOM32 (38-pin)
┌─────────────────────────────────────────┐
│  GND (multiple pins available)          │ ← Connect to GND
│  3V3 (power)                            │
│  EN                                      │
│  IO34                                   │
│  IO35                                   │
│  IO32  ────→  TAIL_ROTOR_IN1           │ (H-Bridge Input 1)
│  IO27  ─────→  TAIL_ROTOR_CCW           │ (Tail Motor PWM CCW)
│  IO27  ────→  TAIL_ROTOR_IN3           │ (H-Bridge Input 3)
│  IO14  ────→  TAIL_ROTOR_IN4           │ (H-Bridge Input 4)
│  IO25  ────→  MAIN_ROTOR_LEFT (PWM)    │ (MOSFET Gate 1)
│  IO26  ────→  MAIN_ROTOR_RIGHT (PWM)   │ (MOSFET Gate 2)
│  IO5                                    │
│  IO4                                    │
│  IO2                                    │
│  IO15                                   │
│  IO13                                   │
│  IO12                                   │
│  IO11 (SPI CLK)                         │
│  IO9  (SPI MOSI)                        │
│  IO10 (SPI MISO)                        │
│  5V                                     │
│  GND                                    │
└─────────────────────────────────────────┘
```

---

## Motor Control Architecture

### 1. Main Rotors (Counter-Rotating)

**Configuration:** Opposite rotation using MOSFET gates
- Both controlled by PWM signals
- Trim adjustment ensures balanced lift
- Frequency: 10 kHz (optimized for smooth operation)

```
PWM Signal (GPIO 25, 26)
    ↓
MOSFET Gate (IRF540N or similar)
    ↓
Motor Gate Circuit
    ↓
Motor Control
```

**Wiring:**
```
ESP32 GPIO25 ──[1k Resistor]──→ MOSFET Q1 Gate
                                    │
                                   Drain ──→ Motor Left (+)
                                   Source ──→ GND
                                Motor Left (-) ──→ GND

ESP32 GPIO26 ──[1k Resistor]──→ MOSFET Q2 Gate
                                    │
                                   Drain ──→ Motor Right (+)
                                   Source ──→ GND
                                Motor Right (-) ──→ GND
```

### 2. Tail Rotor (Dual PWM Control)

**Configuration:** Two separate PWM pins for bidirectional control
- Both tail motors enabled by default (cheap helicopter)
- One PWM pin for clockwise rotation
- One PWM pin for counterclockwise rotation
- Simple on/off control via PWM duty cycle

```
PWM Signal (GPIO 32) for Clockwise
    ↓
Motor Terminal A (+)
    ↓
Motor Rotation CW


PWM Signal (GPIO 27) for Counter-Clockwise
    ↓
Motor Terminal B (+)
    ↓
Motor Rotation CCW
```

---

## Complete Wiring Schematic

### Power Distribution:
```
Power Supply (5-12V)
    ├─→ [Voltage Regulator] ──→ 3.3V ──→ ESP32 3V3 Pin
    ├─→ ESP32 5V Pin (if 5V available)
    └─→ Motors & H-Bridge

GND (Common Ground)
    ├─→ ESP32 GND
    ├─→ Motors GND
    ├─→ H-Bridge GND
    └─→ MOSFET Source
```

### Signal Connections:

```
                ┌──────────────────────────────────┐
                │      ESP32 WROOM32               │
                │                                  │
                │  GPIO25 ──[1k Ω]──→ Q1 Gate    │
                │  GPIO26 ──[1k Ω]──→ Q2 Gate    │
                │                                  │
                │  GPIO32 ──────→ L298N IN1       │
                │  GPIO27 ──────→ Tail Motor CCW    │
                │  GPIO27 ──────→ L298N IN3       │
                │  GPIO14 ──────→ L298N IN4       │
                │                                  │
                │  GND ─────────→ Common GND      │
                │  3V3 ─────────→ Power           │
                └──────────────────────────────────┘
                        ↓    ↓    ↓    ↓
            ┌─────────────────────────────┐
            │   MOSFET Drivers (Q1, Q2)   │
            │   ┌──[Drain]──→ Motor +    │
            │   └──[Source]→ GND         │
            └─────────────────────────────┘
                        ↓
            ┌─────────────────────────────┐
            │  Main Rotor Motors          │
            │  (Left: Counter-clock)      │
            │  (Right: Clockwise)         │
            └─────────────────────────────┘
                        
            ┌─────────────────────────────┐
            │    L298N H-Bridge Module    │
            │  IN1→ Forward/Reverse       │
            │  IN2→ Forward/Reverse       │
            │  IN3→ Forward/Reverse       │
            │  IN4→ Forward/Reverse       │
            │      ↓                      │
            │    Tail Motor Control       │
            └─────────────────────────────┘
```

---

## Step-by-Step Wiring Instructions

### Step 1: Prepare ESP32
1. Connect ESP32 to USB for programming
2. Verify board selection: `esp32dev` in PlatformIO

### Step 2: Main Rotor MOSFET Connections
```
For each motor:
1. Connect GPIO pin (25/26) → 1kΩ resistor → MOSFET Gate
2. Connect MOSFET Drain → Motor positive (+)
3. Connect MOSFET Source → GND
4. Connect Motor negative (-) → GND
5. Add 10kΩ pull-down from Gate to GND (optional but recommended)
```

### Step 3: Tail Rotor PWM Connections
```
1. GPIO32 (ESP32) → Tail Motor A (clockwise motor) (+)
2. GPIO27 (ESP32) → Tail Motor B (counter-clockwise motor) (+)
3. Both motor (-) → GND
4. Both motors enabled by default (cheap helicopter design)
```

### Step 4: Power Connections
```
1. Connect Power Supply GND to ESP32 GND (common reference)
2. Connect Power Supply to MOSFET source connections
3. Connect Power Supply to H-Bridge power input
4. If using separate 3.3V supply: Connect to ESP32 3V3
```

### Step 5: Safety Measures
```
1. Add 100µF capacitor across power and GND near ESP32
2. Add 100µF capacitor across motor power
3. Verify all connections before powering
4. Test with no propellers attached first
```

---

## Software Setup

### 1. Install PlatformIO
- Install VS Code Extension: PlatformIO IDE

### 2. Configure platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    me-no-dev/ESP Async WebServer@^1.2.3
    me-no-dev/AsyncTCP@^1.1.1
monitor_speed = 115200
build_flags = -O2
```

### 3. Upload Code
1. Connect ESP32 via USB
2. Click PlatformIO "Upload" button
3. Watch serial monitor for startup messages

---

## Using the Controller

### Connection Steps:
1. Power on ESP32
2. On your phone/computer, go to WiFi settings
3. Connect to network: **RC_Helicopter_Control**
4. Password: **12345678**
5. Open web browser → Go to **http://192.168.4.1**

### Interface Controls:

| Control | Function | Range |
|---------|----------|-------|
| Main Rotor Speed | Lift control | 0-100% |
| Tail Rotor Position | Forward/Reverse/Stop | Left ↔ Right |
| Main Rotor Trim | Balance left/right rotors | 0-100% |
| Tail Rotor Trim | Fine-tune tail response | -50 to +50 |

### Control Tips:
- Start with main rotor speed at 0%, increase gradually
- Use tail rotor trim if helicopter drifts
- Save trim settings for consistent flying
- Reset all controls if behavior becomes erratic

---

## Testing Checklist

- [ ] ESP32 boots and prints to serial monitor
- [ ] WiFi hotspot appears (RC_Helicopter_Control)
- [ ] Can connect from phone/computer
- [ ] Web interface loads at 192.168.4.1
- [ ] Main rotor PWM signal outputs (test with multimeter)
- [ ] Tail rotor H-bridge outputs work
- [ ] Motors respond to slider changes
- [ ] Trim settings save after restart
- [ ] All motors stop immediately when sliders set to zero

---

## Troubleshooting

### WiFi Not Appearing:
- Check Serial Monitor for error messages
- Try power cycling ESP32
- Verify WiFi code in main.cpp

### Motors Not Responding:
- Verify GPIO pin connections
- Check power supply voltage
- Test MOSFET/H-Bridge connections with multimeter
- Verify PWM frequency (should be 10kHz)

### Web Interface Not Loading:
- Verify IP: 192.168.4.1 (may differ, check serial monitor)
- Clear browser cache
- Try different browser
- Check WiFi connection strength

### Trim Not Saving:
- Verify Preferences library initialization
- Check ESP32 flash memory availability
- Monitor Serial for save confirmation messages

---

## Performance Optimization Notes

**Why 10 kHz PWM:**
- Smooth motor control without audible noise
- Balanced between responsiveness and efficiency
- Optimal for most DC motor types

**Async Web Server Benefits:**
- Non-blocking control updates
- Multiple simultaneous connections possible
- Responsive UI even with motor changes

**Memory Optimization:**
- Embedded HTML/CSS/JS (no external files needed)
- Lightweight JSON parsing
- Minimal RAM overhead

---

## Safety Warnings

⚠️ **IMPORTANT:**
1. Always disconnect propellers during testing
2. Never exceed motor rated voltage
3. Keep hands away from rotating propellers
4. Test in open space away from obstacles
5. Always stop motors before accessing wiring
6. Use appropriate power supply for motor specifications

---

## Pin Configuration Summary

## Pin Configuration Summary

| ESP32 Pin | Function | Connected To |
|-----------|----------|---------------|
| GPIO25    | PWM Ch0  | MOSFET Q1 Gate (Main Left) |
| GPIO26    | PWM Ch1  | MOSFET Q2 Gate (Main Right) |
| GPIO32    | PWM Ch2  | Tail Motor A (Clockwise) |
| GPIO27    | PWM Ch3  | Tail Motor B (Counter-CW) |
| 3V3       | Power    | Logic Supply |
| GND       | Ground   | Common Return |

---

## Code Structure

```
main.cpp
├── PIN DEFINITIONS (GPIO mapping)
├── PWM CONFIGURATION (10kHz setup)
├── CONTROL VARIABLES (speed, position, trim)
├── HTML/CSS/JS INTERFACE (embedded)
├── SETUP FUNCTIONS
│   ├── setupPreferences() - EEPROM init
│   ├── setupWiFi() - AP mode configuration
│   ├── setupPWM() - Motor PWM setup
│   └── setupWebServer() - HTTP endpoints
├── CONTROL FUNCTIONS
│   ├── setMainRotorSpeed() - PWM control
│   ├── setTailRotorPosition() - H-Bridge control
│   └── loadTrim()/saveTrim() - Persistence
└── setup() & loop() - Main execution
```

---

## Power Consumption Estimates

| Component | Current | Notes |
|-----------|---------|-------|
| ESP32 | ~80mA | WiFi active |
| Main Motors | Variable | Depends on load |
| Tail Motor | Variable | Depends on load |
| H-Bridge (idle) | ~10mA | No active load |
| MOSFET (idle) | ~1mA | Per device |

---

## Modification Guide

### Change WiFi Credentials:
In `setupWiFi()`:
```cpp
WiFi.softAP("NEW_SSID", "NEW_PASSWORD", 1, false, 4);
```

### Adjust PWM Frequency:
In `setupPWM()`:
```cpp
const int PWM_FREQUENCY = 5000;  // Change to 5kHz or 20kHz
```

### Add New GPIO Pins:
1. Define constant: `const int NEW_PIN = 35;`
2. Add PWM channel: `ledcSetup(PWM_CHANNEL_N, 10000, 8);`
3. Attach pin: `ledcAttachPin(NEW_PIN, PWM_CHANNEL_N);`
4. Update web interface with new control

---

## Version History

**v1.0 - Initial Release**
- WiFi hotspot control
- PWM for main rotors
- H-Bridge for tail rotor
- Trim adjustments
- Persistent storage

---

## Support & Resources

- ESP32 Datasheet: https://www.espressif.com
- PlatformIO Docs: https://docs.platformio.org
- Arduino Core for ESP32: https://github.com/espressif/arduino-esp32
- L298N H-Bridge Guide: https://www.hobbytronics.co.uk

---

**Last Updated:** 2026-05-15
**Status:** Production Ready
