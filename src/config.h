#pragma once

// ─── OBD-II UART (wired ELM327 via Serial2) ───────────────────────────────────
// ESP32-P4 has no Bluetooth; connect ELM327 adapter via UART
#define OBD_RX_PIN      16                     // GPIO connected to ELM327 TX
#define OBD_TX_PIN      17                     // GPIO connected to ELM327 RX
#define OBD_BAUD        38400

// ─── Polling ──────────────────────────────────────────────────────────────────
#define PID_POLL_INTERVAL_MS    100            // ms between each PID query

// ─── Units ────────────────────────────────────────────────────────────────────
// Set to 1 for mph, 0 for km/h
#define SPEED_UNIT_MPH          1

// ─── Thresholds (for color coding) ────────────────────────────────────────────
#define COOLANT_WARN_C          100
#define COOLANT_CRIT_C          110
#define OIL_WARN_C              120
#define OIL_CRIT_C              135
#define ENGINE_LOAD_WARN_PCT    70
#define ENGINE_LOAD_CRIT_PCT    90

// ─── Display ──────────────────────────────────────────────────────────────────
#define DISPLAY_WIDTH           1280
#define DISPLAY_HEIGHT          720

// ─── PIDs ─────────────────────────────────────────────────────────────────────
#define PID_ENGINE_RPM          0x0C
#define PID_VEHICLE_SPEED       0x0D
#define PID_COOLANT_TEMP        0x05
#define PID_ENGINE_LOAD         0x04
#define PID_THROTTLE_POS        0x11
#define PID_INTAKE_AIR_TEMP     0x0F
#define PID_OIL_TEMP            0x5C
#define PID_CTRL_MODULE_VOLTAGE 0x42
