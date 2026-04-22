#pragma once

// ─── Bluetooth ────────────────────────────────────────────────────────────────
// MAC address of the Veepeak Mini OBD-II adapter (format: "XX:XX:XX:XX:XX:XX")
#define OBD_BT_MAC      "00:00:00:00:00:00"   // TODO: replace with your adapter MAC
#define OBD_BT_NAME     "VEEPEAK"              // fallback name for discovery

// ─── Polling ──────────────────────────────────────────────────────────────────
#define PID_POLL_INTERVAL_MS    100            // ms between each PID query

// ─── Units ────────────────────────────────────────────────────────────────────
// Set to 1 for mph, 0 for km/h
#define SPEED_UNIT_MPH          0

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
#define LVGL_DRAW_BUF_LINES     20             // lines per LVGL draw buffer

// ─── PIDs ─────────────────────────────────────────────────────────────────────
#define PID_ENGINE_RPM          0x0C
#define PID_VEHICLE_SPEED       0x0D
#define PID_COOLANT_TEMP        0x05
#define PID_ENGINE_LOAD         0x04
#define PID_THROTTLE_POS        0x11
#define PID_INTAKE_AIR_TEMP     0x0F
#define PID_OIL_TEMP            0x5C
#define PID_CTRL_MODULE_VOLTAGE 0x42
