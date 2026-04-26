#pragma once
#include <Arduino.h>

// ─── Shared LoRa state ────────────────────────────────────────────────────────

#define LORA_MAX_MESSAGES  32
#define LORA_MSG_MAX_LEN   120

enum LoraState : uint8_t {
    LORA_STATE_INIT       = 0,  // not yet attempted
    LORA_STATE_CONNECTING = 1,  // init in progress
    LORA_STATE_READY      = 2,  // init succeeded, TX/RX active
    LORA_STATE_ERROR      = 3,  // init failed / module absent
};

struct LoraMessage {
    char     text[LORA_MSG_MAX_LEN];
    int8_t   rssi;          // dBm  (0 = not available for outbound)
    int8_t   snr;           // dB
    bool     isMine;
    uint32_t timestamp_ms;
};

struct LoraData {
    LoraState  state;
    char       stateStr[48];    // human-readable status for the UI
    int32_t    lastRssi;
    int32_t    lastSnr;
    int        msgCount;        // total messages in ring buffer (capped at LORA_MAX_MESSAGES)
    LoraMessage messages[LORA_MAX_MESSAGES];
};

extern LoraData          gLoraData;
extern SemaphoreHandle_t gLoraMutex;

// Start the background FreeRTOS task. Safe to call before hardware is attached.
void loraManagerStart();

// Queue a text message for transmission. Returns false if not connected.
bool loraSend(const char* text);
