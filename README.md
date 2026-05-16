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

## Default Configuration

| Setting | Value | Notes |
|---------|-------|-------|
| WiFi SSID | `RC_Helicopter_Control` | AP mode (no internet required) |
| WiFi Password | `12345678` | Change in code if desired |
| WiFi IP | `192.168.4.1` | Fixed AP IP |
| **Main Trim (NVS default)** | **128 (50%)** | **Neutral — no differential applied** |
| Autoland Timeout | 3000 ms | After 3s no client activity, autoland begins |
| PWM Frequency | 10 kHz | LEDC setup for stable motor control |
| PWM Resolution | 8-bit (0–255) | Balanced responsiveness |

## Build & Upload

### Requirements

- **PlatformIO** (VS Code extension or CLI)
- **ESP32 Board:** ESP32 WROOM32 DevKit (or compatible)
- **Dependencies:** Automatically resolved by `platformio.ini`
  - `ESP Async WebServer` (1.2.3)
  - `AsyncTCP` (1.1.1)

### Steps

1. **Open the project** in VS Code with PlatformIO installed.
2. **Connect ESP32** via USB.
3. **Build & Upload:**
   ```bash
   platformio run --target upload --upload-port COM4
   ```
   (Replace `COM4` with your port, or use `auto` for auto-detection)
4. **Monitor Serial Output** (optional):
   ```bash
   platformio device monitor --baud 115200
   ```

## Usage

### Connect and Control

1. **Enable WiFi** on your device (phone/tablet).
2. **Connect to AP:** Select `RC_Helicopter_Control` and enter password `12345678`.
3. **Open Web Interface:** In a browser, navigate to `http://192.168.4.1`.
4. **Control:**
   - **Left Stick:** Drag vertically to adjust throttle (0–100%). Does NOT auto-center on release.
   - **Right Stick:** Drag to any direction for pitch/roll control. Auto-centers on release.
   - **Yaw Slider:** Adjust left/right to control tail rotor rotation.
   - **MAIN +/−:** Trim buttons to adjust left/right main rotor balance.
   - **Save Trim:** Persist current trim value to NVS (survives power cycle).
   - **Reset:** Clear all controls to zero; does not affect saved trim.

### Trim Behavior

- **Default Trim:** 50% (neutral) — both main motors spin at the same speed. **No trim is applied by default.**
- **Adjust Trim:** Use MAIN +/− buttons to compensate for motor speed imbalance:
  - Increase trim (MAIN +) → left motor spins slightly faster.
  - Decrease trim (MAIN −) → right motor spins slightly faster.
- **Save Trim:** Click "Save Trim" to store the current trim value. It persists after power-off.
- **Reset Trim:** Manually adjust trim back to 50% and save, or power-cycle after clearing NVS (advanced).

## WebSocket Protocol

The UI communicates with the ESP32 via WebSocket at `ws://192.168.4.1/ws`.

### Client → Server Messages

```json
{
  "type": "control",
  "throttle": 0,    // 0–100 (main rotor speed)
  "yaw": 0,         // –100 to 100 (tail rotor position)
  "pitch": 0,       // –100 to 100 (pitch control)
  "roll": 0,        // –100 to 100 (roll control)
  "trim": 50        // 0–100 (main rotor trim, 50 = neutral)
}
```

### Client → Server Command

```json
{
  "type": "saveTrim",
  "trim": 50        // Save current trim to NVS
}
```

## Troubleshooting

### UI Does Not Appear or Is Zoomed Incorrectly

- Verify WiFi connection is stable (signal bars visible in phone settings).
- Try refreshing the page or opening in a private/incognito window.
- Check that the address bar shows `http://192.168.4.1/` (not `https://`).
- Mobile browsers may cache outdated HTML; clear cache and reload.

### Motors Do Not Respond or Have Lag

- **Lag Issue:** Ensure WebSocket connection is active (status shows "CONNECTED").
- **Motor Not Spinning:** Check PWM pin connections to motor drivers and verify GPIO pin numbers match hardware wiring.
- **Inconsistent Speed:** Adjust main rotor trim using MAIN +/− buttons until both motors spin at equal speed when throttle is held steady.
- **Default Trim:** On first power-up, trim defaults to neutral (50%) with no differential. If motors seem unbalanced, try adjusting trim with MAIN +/− buttons.

### Tail Rotor Not Working

- **No Response:** Verify GPIO 32 (CW) and GPIO 27 (CCW) are connected to motor driver inputs and power supply is sufficient.
- **Only One Direction:** Check that both PWM channels are properly configured in the firmware (PWM_CHANNEL_TAIL_CW = 2, PWM_CHANNEL_TAIL_CCW = 3).
- **Oscillation or Chatter:** Increase deadband (currently ±5, adjustable in `setTailRotorPosition` function).

### Connection Drops / Autoland Triggers

- **WiFi Dropout:** Move the controller closer to the ESP32 AP or increase transmit power in WiFi settings (advanced).
- **Autoland Timeout:** Currently set to 3 seconds. After 3 seconds of no activity, the helicopter descends at a fixed rate. To adjust, modify `CONNECTION_TIMEOUT` (in milliseconds) in the firmware.

## File Structure

```
Rc helicopter custom controller/
├── platformio.ini                 # PlatformIO project configuration
├── README.md                      # This file
├── src/
│   └── main.cpp                   # Firmware (WebSocket server + embedded HTML)
├── include/
│   └── README
├── lib/
│   └── README
└── test/
    └── README
```

## API Reference

### Firmware Functions

| Function | Parameters | Returns | Description |
|----------|-----------|---------|-------------|
| `setupWiFi()` | — | void | Initialize WiFi in AP mode at 192.168.4.1 |
| `setupPWM()` | — | void | Configure LEDC PWM for all 4 motor channels |
| `setupWebServer()` | — | void | Start HTTP server and WebSocket endpoint |
| `setMainRotorSpeed(uint8_t speed)` | speed: 0–255 | void | Set main rotor PWM with trim differential |
| `setTailRotorPosition(int8_t pos)` | pos: –127 to 127 | void | Set tail rotor CW/CCW PWM |
| `loadTrim()` | — | void | Read trim from NVS (default 128 = neutral) |
| `saveTrim()` | — | void | Write trim to NVS |
| `handleControlMessage(const String &msg)` | msg: JSON | void | Parse and apply control message from WebSocket |

## Performance Notes

- **Input Latency:** Estimated ~50–150 ms end-to-end (WiFi + WebSocket + PWM write).
- **Motor PWM Resolution:** 8-bit (0–255) at 10 kHz.
- **Autoland Performance:** Smooth descent (~12 PWM units per 200 ms). Adjust `AUTOLAND_DECREMENT` to change descent rate.

## Future Enhancements

- Add remote telemetry (battery voltage, motor current) via WebSocket.
- Implement multi-channel trim (per-motor).
- Add voice feedback or haptic feedback on mobile control.
- Implement model storage (save/load preset control profiles).

## License & Notes

This project is provided as-is for educational and hobby purposes. Ensure all local regulations for RC helicopter operation are followed.

---

**Questions or Issues?** Check the troubleshooting section above, or review the firmware comments in [src/main.cpp](src/main.cpp) for detailed implementation notes.

## Security Notes

⚠️ **Important:**
- WiFi password is basic (change if needed)
- No encryption enabled (local network only)
- Always supervise operation
- Keep propellers away during testing
- Verify motor connections before powering

---

## Advanced Configuration

### Change WiFi Credentials:
Edit `src/main.cpp` line ~165:
```cpp
WiFi.softAP("YOUR_SSID", "YOUR_PASSWORD", 1, false, 4);
```

### Adjust PWM Frequency:
Edit `src/main.cpp` line ~26:
```cpp
const int PWM_FREQUENCY = 10000;  // Change value here (Hz)
```

### Modify Pin Assignment:
Edit constants at top of `src/main.cpp`:
```cpp
const int MAIN_ROTOR_LEFT = 25;   // GPIO pin
const int TAIL_ROTOR_IN1 = 32;    // GPIO pin
// etc...
```

### Add Motor Monitoring:
Uncomment telemetry in loop() for serial output every 5 seconds.

---

## Files Structure

```
Rc helicopter custom controller/
├── platformio.ini              # PlatformIO configuration
├── README.md                   # This file
├── WIRING_GUIDE.md            # Detailed wiring instructions
├── src/
│   └── main.cpp               # Main firmware code
├── include/
│   └── README                 # Include path info
├── lib/
│   └── README                 # Library path info
└── test/
    └── README                 # Test directory
```

---

## Compilation & Upload

### Build:
```bash
platformio run
```

### Upload to ESP32:
```bash
platformio run --target upload
```

### Monitor Serial Output:
```bash
platformio device monitor
```

### Build & Upload Combined:
```bash
platformio run --target upload && platformio device monitor
```

---

## Performance Tips

1. **PWM Frequency at 10 kHz** - Optimal balance for smooth control
2. **Async Web Server** - Non-blocking for responsive UI
3. **Minimal JSON Parsing** - Lightweight string operations
4. **NVRAM Storage** - Persistent trim without SD card
5. **Optimized O2 Flag** - Compiler optimization for speed

---

## Motor Control Theory

### Main Rotors (Opposite Rotation):
- Both controlled by independent PWM signals
- Trim adjustment balances speed between rotors
- Creates lift force for helicopter

### Tail Rotor (Bidirectional):
- H-Bridge allows forward/reverse motion
- Provides yaw control (rotation)
- Dead zone (-5 to +5) prevents drift

---

## Battery/Power Recommendations

| Motor Type | Typical Voltage | Power Draw |
|-----------|-----------------|-----------|
| Micro DC | 3.7V (1S LiPo) | 0.5-1A |
| Standard DC | 6-12V | 1-3A |
| High Performance | 12V+ | 3-5A+ |

**ESP32 Power:**
- Logic: 3.3V @ 80mA
- Typical 1A power supply sufficient
- Separate supply for motors recommended

---

## Noise Considerations

- 10 kHz PWM is inaudible to humans
- Motors may have inherent noise
- Propellers generate most sound
- Consider thrust-to-noise tradeoff

---

## Future Enhancements

Possible additions:
- [ ] Accelerometer/Gyro stabilization
- [ ] Altitude hold with barometer
- [ ] Dual-frequency WiFi support
- [ ] OTA firmware updates
- [ ] Smartphone app (instead of web)
- [ ] Failsafe mechanisms
- [ ] Flight telemetry logging
- [ ] Video camera integration

---

## License & Credits

Built for DIY RC enthusiast projects.
Uses ESP-IDF and Arduino core libraries.


## Support

For issues:
1. Check [WIRING_GUIDE.md](WIRING_GUIDE.md) for connections
2. Monitor serial output for errors
3. Verify all GPIO connections
4. Check power supply voltage
5. Try reset/reupload firmware

---

**Status:** ✅ Production Ready  
**Version:** 1.0  
**Last Updated:** 2026-05-15
#   T o y - C h e a p - R C - h e l i c o p t e r - c o n t r o l l - v i a - E P S 3 2 
 
 #   T o y - C h e a p - R C - h e l i c o p t e r - c o n t r o l l - v i a - E P S 3 2 
 
 
