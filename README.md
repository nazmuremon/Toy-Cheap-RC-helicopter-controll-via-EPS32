# ESP32 RC Helicopter Remote Control System

A low-latency WiFi-based remote control transmitter for RC helicopters, built on the ESP32 with an embedded responsive web UI and WebSocket control protocol.

## Features

- **WebSocket-based Control:** Real-time motor PWM updates with minimal input lag via WebSocket (`/ws`).
- **Dual-Mode Motor Control:**
  - Main rotor: Single throttle control with left/right trim differential balance.
  - Tail rotor: Dual PWM (CW/CCW) for direct yaw control without centering.
- **Responsive Mobile UI:**
  - Left stick: Non-centering throttle slider (vertical movement only).
  - Right stick: Centered directional control (pitch/roll with auto-center on release).
  - Dedicated yaw slider for tail rotor control.
  - Fixed viewport layout with safe-area insets — no unwanted zoom or scrolling on mobile.
- **Persistent Trim:** Main rotor trim saved to ESP32 NVS (non-volatile storage). **Default: neutral (no differential).**
- **Autoland Safety:** If client disconnects for 3 seconds, motors smoothly reduce to zero for safe landing.
- **Optimized PWM:** 10 kHz LEDC PWM at 8-bit resolution for smooth motor response.

## Hardware

### Components

| Component | GPIO | Purpose |
|-----------|------|---------|
| Main Rotor Left | GPIO 25 | PWM for left main motor |
| Main Rotor Right | GPIO 26 | PWM for right main motor |
| Tail Rotor CW | GPIO 32 | PWM for clockwise tail rotor |
| Tail Rotor CCW | GPIO 27 | PWM for counterclockwise tail rotor |

### Motor Control Logic

**Main Rotor Speed (Throttle):**
- Input: 0–100% (from left stick)
- Maps to PWM duty: 0–255
- Applied differential trim based on stored `main_rotor_trim` value (0–255, where **128 = neutral**):
  - Left motor = base_speed + trim_offset
  - Right motor = base_speed - trim_offset
  - When trim = 128, offset = 0 → both motors equal (no trim applied)

**Tail Rotor Position (Yaw):**
- Input: –100 to 100 (from yaw slider)
- Maps to position: –127 to 127
- Control logic:
  - If position > 5: CW PWM active, CCW PWM = 0
  - If position < –5: CCW PWM active, CW PWM = 0
  - If –5 ≤ position ≤ 5: both PWM outputs = 0 (idle)
