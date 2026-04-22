# Tab5 OBD-II Dashboard — Project Plan

## Overview
Build a PlatformIO/Arduino firmware for the M5Stack Tab5 that connects via Bluetooth Classic to a Veepeak Mini OBD-II adapter (ELM327-based), polls key vehicle PIDs, and renders a real-time dashboard UI using LVGL on the Tab5's 1280×720 touchscreen.

---

## Tech Stack & Constraints

### Hardware
- M5Stack Tab5 (ESP32-P4 main SoC, ESP32-C6 handles Wi-Fi — **Bluetooth Classic is on the ESP32-P4 itself**)
- Display: 5" 1280×720 IPS, driver IC is either ILI9881C or ST7123 (check sticker on back) via MIPI-DSI
- Touch: GT911 or ST7123 integrated (I2C, INT on G23)
- OBD adapter: Veepeak Mini — ELM327 protocol over **Bluetooth Classic SPP** (not BLE)

### Software
- Framework: Arduino via PlatformIO
- Base `platformio.ini` from M5Stack docs (use exactly as provided, adding lib_deps as needed):

```ini
[env:esp32p4_pioarduino]
platform = https://github.com/pioarduino/platform-espressif32.git#54.03.21
upload_speed = 1500000
monitor_speed = 115200
build_type = debug
framework = arduino
board = esp32-p4-evboard
board_build.mcu = esp32p4
board_build.flash_mode = qio
build_flags =
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    https://github.com/M5Stack/M5Unified.git
    https://github.com/M5Stack/M5GFX.git
    lvgl/lvgl @ ^9.5.0
```

- UI: LVGL v9 rendered via M5GFX as the display driver backend
- Bluetooth: Arduino `BluetoothSerial` library (ESP32 Classic BT SPP)

---

## Architecture

```
[Veepeak OBD-II] --BT Classic SPP--> [ESP32-P4 BluetoothSerial]
                                              |
                                       [ELM327 Layer]
                                       (send AT cmds,
                                        parse PID responses)
                                              |
                                       [PID Poller Task]
                                       (FreeRTOS task,
                                        round-robin PIDs)
                                              |
                                       [Data Model]
                                       (shared struct,
                                        mutex-protected)
                                              |
                                       [LVGL UI Task]
                                       (lv_timer or main loop,
                                        reads data model,
                                        updates widgets)
                                              |
                                     [M5GFX flush callback]
                                              |
                                    [Tab5 MIPI-DSI Display]
```

---

## Implementation Phases

### Phase 1 — Project Scaffold & Display Verification
- Set up PlatformIO project with the base `platformio.ini`
- Initialize M5Unified (`M5.begin()`) — this handles display, touch, power init
- Initialize LVGL, wire up M5GFX as the LVGL display driver (flush callback, input driver for touch)
- Render a static LVGL test screen to verify display + touch are working
- **Key note:** M5Unified abstracts the ILI9881C vs ST7123 driver difference — use it, don't drive the display directly

### Phase 2 — Bluetooth & ELM327 Communication
- Implement a `ELM327Client` class wrapping `BluetoothSerial`
- Pair/connect to the Veepeak by MAC address (hardcode initially, make configurable later)
- Implement AT command initialization sequence:
  ```
  ATZ        (reset)
  ATE0       (echo off)
  ATL0       (linefeeds off)
  ATS0       (spaces off)
  ATSP0      (auto protocol)
  ```
- Implement `queryPID(uint8_t pid)` → sends `01 XX` command, reads and parses hex response
- Add timeout and retry handling — ELM327 adapters are slow and chatty

### Phase 3 — PID Polling

PIDs to poll (Mode 01, all standard OBD-II):

| PID  | Name                   | Formula              | Unit    |
|------|------------------------|----------------------|---------|
| 0x0C | Engine RPM             | ((A*256)+B)/4        | RPM     |
| 0x0D | Vehicle Speed          | A                    | km/h or mph |
| 0x05 | Coolant Temp           | A - 40               | °C      |
| 0x04 | Engine Load            | A * 100/255          | %       |
| 0x11 | Throttle Position      | A * 100/255          | %       |
| 0x0F | Intake Air Temp        | A - 40               | °C      |
| 0x5C | Oil Temp               | A - 40               | °C      |
| 0x42 | Control Module Voltage | ((A*256)+B)/1000     | V       |

- Run polling in a dedicated FreeRTOS task (core 1)
- Round-robin through the PID list with a configurable poll interval (~100ms per PID)
- Store results in a mutex-protected `VehicleData` struct

### Phase 4 — LVGL Dashboard UI

Layout for 1280×720 — landscape orientation, dark automotive theme:

```
┌─────────────────────────────────────────────────────────────────┐
│  ┌──────────┐   ┌──────────────────────────┐   ┌──────────┐   │
│  │          │   │                          │   │          │   │
│  │  COOLANT │   │    RPM ARC GAUGE         │   │  OIL     │   │
│  │  TEMP    │   │    (large, center)       │   │  TEMP    │   │
│  │          │   │                          │   │          │   │
│  └──────────┘   └──────────────────────────┘   └──────────┘   │
│                                                                  │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   │
│  │  SPEED   │   │  ENGINE  │   │ THROTTLE │   │  BATT    │   │
│  │  km/h    │   │  LOAD %  │   │  POS %   │   │  VOLT    │   │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘   │
│                                                                  │
│  [BT STATUS]  [PROTOCOL]  [INTAKE TEMP]        [TAP FOR MENU]  │
└─────────────────────────────────────────────────────────────────┘
```

LVGL widgets to use:
- `lv_arc` for RPM (large center gauge with needle-style arc)
- `lv_label` for numeric values
- `lv_bar` or `lv_arc` for load/throttle as progress arcs
- `lv_obj` containers with styled backgrounds for each tile
- Color coding: green → yellow → red thresholds for temps and load

### Phase 5 — BT Connection Management & UX Polish
- Splash/connecting screen while BT pairs
- Status bar showing BT connection state and OBD protocol detected
- Tap anywhere → settings overlay (units toggle mph/km/h, brightness slider)
- Graceful reconnect loop if BT drops
- Screen brightness control via M5Unified (mapped to INA226 power awareness optionally)

---

## Key Gotchas to Watch For

1. **Bluetooth Classic on ESP32-P4** — confirm the pioarduino platform version supports `BluetoothSerial` on P4; if not, may need to implement SPP via ESP-IDF BT stack directly or find a P4-compatible BT library
2. **LVGL + M5GFX integration** — use M5GFX's `LGFX_Sprite` as LVGL draw buffer; the M5Stack community has working examples for M5Unified+LVGL on other devices that can serve as reference
3. **Display driver variance** — M5Unified handles ILI9881C vs ST7123 automatically, but pin your M5Unified lib version to latest to get ST7123 support
4. **ELM327 timing** — the Veepeak is slow; don't send a new command before receiving the `>` prompt from the previous one
5. **FreeRTOS task pinning** — pin the BT/polling task to core 0, LVGL rendering to core 1 (or vice versa, but be consistent to avoid contention)

---

## Suggested File Structure

```
src/
  main.cpp           - setup(), loop(), LVGL tick
  elm327.h/.cpp      - ELM327Client class (BT + AT protocol)
  pid_poller.h/.cpp  - FreeRTOS task, VehicleData struct
  ui/
    dashboard.h/.cpp - LVGL screen init and widget update
    theme.h          - colors, fonts, style constants
  config.h           - BT MAC, PID list, thresholds, units
```
