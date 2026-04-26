#include "lora_manager.h"
#include "config.h"
#include <Arduino.h>
#include <rak3172_p2p.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cstdio>

// ─── Shared state ─────────────────────────────────────────────────────────────
LoraData          gLoraData   = {};
SemaphoreHandle_t gLoraMutex  = nullptr;

// ─── Internal ─────────────────────────────────────────────────────────────────
static RAK3172P2P sLora;
static bool       sInitOk     = false;

// TX queue (simple ring buffer, thread-safe via mutex)
static const int  TX_QUEUE_LEN = 8;
struct TxItem { char text[LORA_MSG_MAX_LEN]; };
static TxItem     sTxQueue[TX_QUEUE_LEN];
static int        sTxHead = 0, sTxTail = 0;

static void set_state(LoraState s, const char* msg) {
    if (xSemaphoreTake(gLoraMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        gLoraData.state = s;
        strncpy(gLoraData.stateStr, msg, sizeof(gLoraData.stateStr) - 1);
        xSemaphoreGive(gLoraMutex);
    }
}

static void push_message(const char* text, int8_t rssi, int8_t snr, bool isMine) {
    if (xSemaphoreTake(gLoraMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

    int idx = gLoraData.msgCount % LORA_MAX_MESSAGES;
    LoraMessage& m = gLoraData.messages[idx];
    strncpy(m.text, text, LORA_MSG_MAX_LEN - 1);
    m.text[LORA_MSG_MAX_LEN - 1] = '\0';
    m.rssi         = rssi;
    m.snr          = snr;
    m.isMine       = isMine;
    m.timestamp_ms = millis();
    gLoraData.lastRssi = rssi;
    gLoraData.lastSnr  = snr;
    gLoraData.msgCount++;

    xSemaphoreGive(gLoraMutex);
}

static bool try_init() {
    set_state(LORA_STATE_CONNECTING, "Connecting to RAK3172...");
    Serial.println("[LORA] Initializing RAK3172 P2P...");

    // Serial1 used for LoRa; Serial2 is OBD
    if (!sLora.init(&Serial1, LORA_RX_PIN, LORA_TX_PIN, RAK3172_BPS_115200)) {
        Serial.println("[LORA] init() failed — module not responding");
        set_state(LORA_STATE_ERROR, "Module not responding");
        return false;
    }
    if (!sLora.config(LORA_FREQ_HZ, LORA_SF, LORA_BW_KHZ, LORA_CR, LORA_PRLEN, LORA_PWR_DBM)) {
        Serial.println("[LORA] config() failed");
        set_state(LORA_STATE_ERROR, "Config failed");
        return false;
    }
    if (!sLora.setMode(P2P_TX_RX_MODE)) {
        Serial.println("[LORA] setMode(TX_RX) failed");
        set_state(LORA_STATE_ERROR, "Mode set failed");
        return false;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "Ready  %.0f MHz  SF%d  BW%d",
             LORA_FREQ_HZ / 1e6f, LORA_SF, LORA_BW_KHZ);
    set_state(LORA_STATE_READY, buf);
    Serial.printf("[LORA] Ready — %s\n", buf);
    return true;
}

// ─── FreeRTOS task ────────────────────────────────────────────────────────────
static void lora_task(void* param) {
    // Initial connection attempt with a short delay for UART to settle
    vTaskDelay(pdMS_TO_TICKS(3000));
    sInitOk = try_init();

    static uint32_t sRetryAt = 0;

    for (;;) {
        // If not connected, retry every 30 s
        if (!sInitOk) {
            if (millis() >= sRetryAt) {
                sRetryAt = millis() + 30000;
                Serial.println("[LORA] Retrying init...");
                sInitOk = try_init();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Drive the library's internal state machine
        sLora.update();

        // Drain TX queue
        while (sTxTail != sTxHead) {
            TxItem& item = sTxQueue[sTxTail];
            Serial.printf("[LORA] Sending: %s\n", item.text);
            if (sLora.print(item.text)) {
                push_message(item.text, 0, 0, /*isMine=*/true);
            } else {
                Serial.println("[LORA] Send failed");
                // Mark error but keep running — hardware may have glitched
                set_state(LORA_STATE_ERROR, "TX failed — retrying init");
                sInitOk = false;
                sRetryAt = millis() + 5000;
            }
            sTxTail = (sTxTail + 1) % TX_QUEUE_LEN;
        }

        // Drain RX queue
        if (sLora.available()) {
            std::vector<p2p_frame_t> frames = sLora.read();
            for (auto& f : frames) {
                // Payload is raw bytes — convert to null-terminated string
                char text[LORA_MSG_MAX_LEN] = {};
                int len = (f.len < LORA_MSG_MAX_LEN - 1) ? f.len : LORA_MSG_MAX_LEN - 1;
                // p2p_frame_t payload is uint8_t[]; treat as ASCII text
                memcpy(text, f.payload, len);
                text[len] = '\0';
                Serial.printf("[LORA] RX  RSSI=%d SNR=%d  \"%s\"\n", f.rssi, f.snr, text);
                push_message(text, (int8_t)f.rssi, (int8_t)f.snr, /*isMine=*/false);
            }
            sLora.flush();
        }

        vTaskDelay(pdMS_TO_TICKS(5));  // yield ~200 Hz, matches library expectation
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void loraManagerStart() {
    gLoraMutex = xSemaphoreCreateMutex();
    gLoraData.state = LORA_STATE_INIT;
    strncpy(gLoraData.stateStr, "Initializing...", sizeof(gLoraData.stateStr) - 1);

    xTaskCreatePinnedToCore(lora_task, "lora_task", 6144, nullptr, 1, nullptr, 0);
    Serial.println("[LORA] Manager task started");
}

bool loraSend(const char* text) {
    if (!sInitOk) {
        Serial.println("[LORA] loraSend: not connected");
        return false;
    }
    int nextHead = (sTxHead + 1) % TX_QUEUE_LEN;
    if (nextHead == sTxTail) {
        Serial.println("[LORA] loraSend: TX queue full");
        return false;
    }
    strncpy(sTxQueue[sTxHead].text, text, LORA_MSG_MAX_LEN - 1);
    sTxQueue[sTxHead].text[LORA_MSG_MAX_LEN - 1] = '\0';
    sTxHead = nextHead;
    return true;
}
