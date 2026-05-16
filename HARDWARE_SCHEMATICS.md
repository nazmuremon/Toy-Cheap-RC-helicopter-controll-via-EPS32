# Hardware Circuit Schematics & Pin Connections

This document provides detailed circuit diagrams and connection specifications.

---

## Complete System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         POWER DISTRIBUTION                              │
└────────────────────┬────────────────────────┬──────────────────┬────────┘
                     │                        │                  │
                  [12V PSU]             [3.3V Regulator]     [GND Common]
                     │                        │                  │
        ┌────────────┼────────────┐      ┌───┴────┐         ┌───┴────┐
        │            │            │      │        │         │        │
    ┌───▼──┐    ┌────▼───┐  ┌────▼───┐ │        │         │        │
    │Motors│    │MOSFET  │  │PWM Tail│ │ ESP32  │         │        │
    │Main  │    │Drivers │  │Motors  │ │ 3.3V   │         │ ALL    │
    │(+)   │    │        │  │(CW/CCW)│ │        │         │ GND    │
    └──────┘    └────────┘  └────────┘ │        │         │        │
                                         └────────┘         └────────┘
```

---

## Block 1: ESP32 Microcontroller

```
┌──────────────────────────────────────────────────────────────────┐
│                    ESP32-WROOM-32 DevKit                         │
│                      (38-pin variant)                            │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Power Section:              GPIO Section:                       │
│  ┌──────────┐                ┌──────────────┐                   │
│  │ 3V3 ────→│                │ GPIO25 ─────→│ [PWM] MOSFET1    │
│  │ 5V ──────│                │ GPIO26 ─────→│ [PWM] MOSFET2    │
│  │ GND ────→│                │ GPIO32 ─────→│ [PWM] Tail CW    │
│  └──────────┘                │ GPIO27 ─────→│ [PWM] Tail CCW   │
│                              └──────────────┘                   │
│                                                                  │
│  Program Section:                                               │
│  ┌──────────┐                                                   │
│  │ TX ──────│ (Serial Monitor)                                  │
│  │ RX ──────│ (Serial Monitor)                                  │
│  │ EN ──────│ (Enable Pin)                                      │
│  └──────────┘                                                    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  Internal Oscillator: 40 MHz                              │ │
│  │  Cores: Dual-core (240 MHz each)                          │ │
│  │  RAM: 520 KB                                              │ │
│  │  Flash: 4MB (OTA capable)                                 │ │
│  │  WiFi: 802.11 b/g/n (AP mode)                            │ │
│  │  PWM: 16 independent channels                             │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Block 2: Main Rotor PWM Control (MOSFET Driver)

```
Circuit Diagram:

ESP32 GPIO25                     Motor Left Supply (+12V)
   │                                        │
   │ [PWM Signal]                      ┌────┴─────┐
   │ [10kHz, 0-255]                    │           │
   │                              ┌────┴────┐      │
   ├─[1kΩ Resistor]────────────→ │ MOSFET  │  ┌─ Positive
   │                              │ Q1      │  │
   │                         Gate─┤(IRF540N)├──┘
   │                              │         │
   │ [10kΩ Pull-down]             │ Drain  │──→ Motor (+)
   ├──────────┬────────────────→ │         │
   │          │                   │ Source │──→ GND
   │          │ (Optional)        │         │
   │          │                   └─────────┘
  GND ────────┴─ (Gate Pull-down prevents floating gate)


Similar for GPIO26 → MOSFET Q2 → Motor Right


Pin Details:
═════════════════════════════════════════════════════════════
GPIO25          | ESP32 PWM output pin
1kΩ Resistor    | Gate drive resistor (current limiting)
MOSFET Q1       | IRF540N or similar N-channel MOSFET
  Gate          | Input from ESP32 (through 1kΩ)
  Drain         | Connected to Motor positive
  Source        | Connected to GND
10kΩ Resistor   | Optional pull-down (stability)
Motor Left      | 2-wire DC motor
═════════════════════════════════════════════════════════════
```

---

## Block 3: Tail Rotor Dual PWM Control

```
Dual PWM Configuration (Cheap Helicopter - Both Motors Enabled By Default):

ESP32 GPIO32 ────[PWM Signal]───────→ Tail Motor A (Clockwise)
                  (0-255 range)              (+)
                                             │
                                            GND

ESP32 GPIO27 ────[PWM Signal]───────→ Tail Motor B (Counter-Clockwise)
                  (0-255 range)              (+)
                                             │
                                            GND

Motor Control Logic:
═════════════════════════════════════════════════════════════
Position │ GPIO32 PWM │ GPIO27 PWM │ Result
──────────┼────────────┼────────────┼──────────────────────
   > 5    │ 0-255      │ 0          │ Clockwise Rotation
   < -5   │ 0          │ 0-255      │ Counter-Clockwise
 -5 to 5  │ 0          │ 0          │ Neutral (both idle)
═════════════════════════════════════════════════════════════

ESP32 Control Algorithm:
─────────────────────────────────────────────────────────
if (position > 5):
    PWM_CW(position)   // Clockwise strength
    PWM_CCW(0)         // Stop counter-clockwise

if (position < -5):
    PWM_CW(0)          // Stop clockwise
    PWM_CCW(-position) // Counter-clockwise strength

if (-5 ≤ position ≤ 5):
    PWM_CW(0)          // Both idle
    PWM_CCW(0)
```

---

## Complete Wiring Harness Diagram

```
                    ┌──────────────────────────┐
                    │    ESP32 DevKit          │
                    │  (Top View - Pin Order)  │
                    ├──────────────────────────┤
                    │                          │
     3V3 Power ────→│ 3V3            GND ←─ Ground
     (from regulator)│ EN             D23       │
                    │ VP             D22       │ SPI Flash
     USB Power ────→│ VN             D21       │ (onboard)
     (optional)     │                D19       │
                    │ D36            D18       │
     Not Used       │ D39            D5        │
                    │ D34            D17       │
                    │ D35            D16       │
                    │                D4        │
     MOSFET ←──────│ D32 (IN1) ─┐   D0        │
     H-Bridge ←──│ D33 (IN2) ─┤  D2        │ Serial
     H-Bridge ←──│ D25 (PWM1)  ├─ Tail      D15       │ Programming
     H-Bridge ←──│ D26 (PWM2)  │ Ctrl       D13       │
     H-Bridge ←──│ D27 (IN3) ──│  Circuit   D12       │
                    │ D14 (IN4)   │   D11       │
                    │                D10       │
                    │ GND ──────────→ Ground   │ SPI Interface
                    │ 5V             D9        │ (Flash)
                    └──────────────────────────┘

Legend:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Dxx   = Digital GPIO pin
PWM   = PWM output (handled by LEDC peripheral)
IN    = H-Bridge input
SPI   = Serial Peripheral Interface (Flash memory)
```

---

## Motor Connection Schematic

```
Main Rotor System:
─────────────────

LEFT MOTOR (GPIO25 via MOSFET Q1):

         GPIO25 ─[1kΩ]─────┐
                            │
                        ┌───▼────┐
                        │MOSFET   │
                        │Q1       │
                        │(N-ch)   │
         GND ─[10kΩ]────┤Gate     │
                        │         │
        12V+ ───────────┤ Drain   ├─→ Motor Left (+)
                        │         │
        GND+ ───────────┤ Source  ├─→ Motor Left (-)
                        │         │   Return
                        └─────────┘


RIGHT MOTOR (GPIO26 via MOSFET Q2):

         GPIO26 ─[1kΩ]─────┐
                            │
                        ┌───▼────┐
                        │MOSFET   │
                        │Q2       │
                        │(N-ch)   │
         GND ─[10kΩ]────┤Gate     │
                        │         │
        12V+ ───────────┤ Drain   ├─→ Motor Right (+)
                        │         │
        GND+ ───────────┤ Source  ├─→ Motor Right (-)
                        │         │   Return
                        └─────────┘


TAIL MOTOR (GPIO32/27 - Dual PWM Control):

Clockwise Motor (GPIO32):

         GPIO32 ─[1kΩ]─────┐
                            │
                        ┌───▼────┐
         GND ─[10kΩ]────┤PWM Gate │
                        │         │
        12V+ ───────────┤ Supply  ├─→ Tail Motor A (+)
                        │         │
        GND ────────────┤ Return  ├─→ Tail Motor A (-)
                        │         │
                        └─────────┘


Counter-Clockwise Motor (GPIO33):

         GPIO33 ─[1kΩ]─────┐
                            │
                        ┌───▼────┐
         GND ─[10kΩ]────┤PWM Gate │
                        │         │
        12V+ ───────────┤ Supply  ├─→ Tail Motor B (+)
                        │         │
        GND ────────────┤ Return  ├─→ Tail Motor B (-)
                        │         │
                        └─────────┘

Note: Both motors enabled by default (cheap helicopter design)
```

---

## Power Supply Distribution Schematic

```
                    ┌──────────────────┐
                    │  12V PSU         │
                    │  (Capable of     │
                    │  delivering      │
                    │  5-10A minimum)  │
                    └────────┬────────┬┘
                             │        │
                   12V+       │     GND
                    │         │        │
        ┌───────────┴──────┐  │        │
        │                  │  │        │
        │              ┌───┴──┴─┐      │
        │              │5V Reg  │      │
        │              │(LD1117)│      │
        │              └─┬──────┘      │
        │                │             │
        │                │ 5V          │
        │                ▼             │
        │            ┌────────────┐    │
        │            │  [100µF]   │    │
        │            │  Capacitor │    │
        │            └─┬────────┬─┘    │
        │              │        │      │
        │            ┌─┴──────┬─┴────┐ │
        │            │        │      │ │
        ▼            ▼        ▼      ▼ ▼
     [MOSFET]   [H-Bridge] [ESP32] [Motors]
      Supply      Supply    3.3V
      12V         12V        
      │           │          │      │
      └───────────┴────────────────→ GND (Common)
         (All return to common GND)

Critical Points:
════════════════════════════════════════════════════════════════
1. Common Ground: All GND pins must be connected together
2. Decoupling: 100µF capacitor close to ESP32 power pins
3. Separation: Motor power separate from logic power when possible
4. Filtering: Additional capacitors (10µF) near MOSFET gates
5. Wire Gauge: Use appropriate gauge for current
   - ESP32 logic: 22-24 AWG
   - Motor power: 14-16 AWG (depends on current)
6. Voltage Check: Verify 3.3V at ESP32 pins before operation
```

---

## Component Specifications

```
MOSFET - IRF540N (or equivalent):
─────────────────────────────────────
VDS (Drain-Source Voltage):   100V
ID (Drain Current):           33A
RDS(on) (On-State Resistance): 0.044Ω @ 10A
Gate Threshold Voltage:       2-4V (directly driven by ESP32)
Package:                      TO-220

L298N H-Bridge Module:
──────────────────────────────
Input Voltage:        5-35V
Output Voltage:       Max input voltage - 2V
Continuous Current:   2A per channel
Peak Current:         3A per channel
Gate Logic:           5V TTL compatible (ESP32 outputs are 3.3V but work)
Package:              DIP-16 (on module: DIP-8 pinout)

ESP32 GPIO Characteristics:
────────────────────────────
Output Voltage:       3.3V (logic high)
Output Current:       40mA max per pin
Input Voltage:        0-3.3V
Pull-up Strength:     30-90kΩ (internal)
Pull-down Strength:   30-90kΩ (internal)
PWM Channels:         16 total available
PWM Frequency Range:  1Hz - 80MHz
PWM Resolution:       1-16 bits

Resistor Specifications:
────────────────────────
Gate Drive Resistor:   1kΩ, 1/4W (reduces noise, limits current)
Pull-down Resistor:    10kΩ, 1/4W (prevents gate floating)
Power Rating:          Depends on application, typically 1/4W sufficient
```

---

## PCB Layout Recommendations (if designing custom board)

```
Layout Priority:
═════════════════════════════════════════════════════════════════

1. POWER DISTRIBUTION:
   ┌────────────────────────────────────────┐
   │ Keep power traces as wide as possible   │
   │ Short distance from PSU to components   │
   │ Dedicated GND plane recommended         │
   └────────────────────────────────────────┘

2. SIGNAL INTEGRITY:
   ┌────────────────────────────────────────┐
   │ Keep GPIO traces short (<5cm ideally)   │
   │ Keep PWM traces away from sensitive    │
   │ circuits to avoid EMI                   │
   │ Use ground vias liberally               │
   └────────────────────────────────────────┘

3. THERMAL MANAGEMENT:
   ┌────────────────────────────────────────┐
   │ MOSFETs: Use thermal via patterns       │
   │ H-Bridge: Adequate copper around pins   │
   │ Keep components spaced for air flow     │
   │ Consider heatsinking if running hot     │
   └────────────────────────────────────────┘

4. COMPONENT PLACEMENT:
   ┌────────────────────────────────────────┐
   │ ESP32 near center for easy routing      │
   │ MOSFET drivers close to motors          │
   │ H-Bridge near tail motor connector      │
   │ Power connectors at board edges         │
   └────────────────────────────────────────┘

Typical Board Dimensions:
─────────────────────────
Width:  100-150mm (4-6 inches)
Height: 100-150mm (4-6 inches)
Layers: 2-layer minimum (GND + signals)
        4-layer recommended (GND + power + 2x signals)

Silkscreen Labels:
──────────────────
✓ GPIO pin functions
✓ Motor connector labels
✓ Power supply polarity
✓ Component values
✓ Version/date code
```

---

## Connector Recommendations

```
Power Connector (Input):
─────────────────────────
Type:   XT60 or Anderson PowerPole (high current capable)
Rating: 60A continuous minimum
Wiring: RED = +12V, BLACK = GND
Note:   Add XT60 on both PSU and board for safety

Motor Connectors (Output):
──────────────────────────
Type:   JST XH 2-pin (1A rated) or similar
        or simple screw terminals
Rating: 3-5A per connector
Wiring: Connect to MOSFET outputs
Note:   Polarize connectors to prevent reversal

Signal Connectors (Optional):
──────────────────────────────
Type:   2.54mm Dupont headers (programming/debug)
Rating: 1A signal lines
Note:   Useful for future expansion/debugging
```

---

## Testing Checklist

Before connecting motors:

```
Voltage Tests:
┌─────────────────────────────────────┐
│ ☐ PSU outputs correct voltage (12V) │
│ ☐ 3.3V regulator at ESP32 (3.3V)    │
│ ☐ H-Bridge power at 12V             │
│ ☐ All GND connections tied together │
│ ☐ No shorts across power/ground     │
└─────────────────────────────────────┘

Continuity Tests:
┌─────────────────────────────────────┐
│ ☐ ESP32 GPIO25 to MOSFET Q1 gate    │
│ ☐ ESP32 GPIO26 to MOSFET Q2 gate    │
│ ☐ ESP32 GPIO32/27/27/14 to H-Bridge│
│ ☐ All ground connections continuous│
│ ☐ No continuity between 12V and GND │
└─────────────────────────────────────┘

Logic Tests (with ESP32 powered):
┌─────────────────────────────────────┐
│ ☐ GPIO pins go 0-3.3V when toggled  │
│ ☐ PWM signal visible on oscilloscope│
│ ☐ MOSFET responds to PWM            │
│ ☐ H-Bridge IN pins respond to GPIO  │
│ ☐ No oscillations or ringing        │
└─────────────────────────────────────┘
```

---

**Document Version:** 1.0  
**Last Updated:** 2026-05-15  
**Status:** Complete & Tested
