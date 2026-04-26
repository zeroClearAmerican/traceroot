#pragma once

// ─── LoRa P2P (RAK3172 via M5-LoRaWAN-RAK, UART) ─────────────────────────────
// Pins TBD — wire the Unit LoRaWAN to available UART GPIOs when hardware arrives
#define LORA_RX_PIN         18          // GPIO connected to RAK3172 TX (placeholder)
#define LORA_TX_PIN         19          // GPIO connected to RAK3172 RX (placeholder)
// US915 P2P defaults — tune before deploying
#define LORA_FREQ_HZ        915000000L  // 915 MHz (US915)
#define LORA_SF             7           // Spreading factor 7 (fastest / shortest range)
#define LORA_BW_KHZ         125         // Bandwidth kHz
#define LORA_CR             0           // Coding rate 4/5
#define LORA_PRLEN          8           // Preamble length
#define LORA_PWR_DBM        17          // TX power dBm

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

// ─── Thresholds (for color coding) — all in °C to match OBD raw values ───────
#define COOLANT_WARN_C          100     // ~212°F
#define COOLANT_CRIT_C          110     // ~230°F
#define OIL_WARN_C              120     // ~248°F
#define OIL_CRIT_C              135     // ~275°F
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
