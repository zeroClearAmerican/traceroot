#include <Arduino.h>
#include <M5Unified.h>
#include <ELMduino.h>

#include "config.h"
#include "ui/dashboard.h"
#include <pid_poller.h>

static M5GFX* gGfx = nullptr;

// ─── Serial OBD (wired UART via Serial2) ──────────────────────────────────────
static ELM327 elm;

static void obd_task(void* param) {
    Serial2.begin(OBD_BAUD, SERIAL_8N1, OBD_RX_PIN, OBD_TX_PIN);

    Serial.println("[OBD] Waiting for ELM327...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (xSemaphoreTake(gVehicleDataMutex, portMAX_DELAY)) {
        gVehicleData.btConnected = true;
        xSemaphoreGive(gVehicleDataMutex);
    }

    if (!elm.begin(Serial2, /*debug=*/false, /*timeout=*/2000)) {
        Serial.println("[OBD] ELM327 handshake failed.");
    } else {
        Serial.println("[OBD] ELM327 ready.");
        if (xSemaphoreTake(gVehicleDataMutex, portMAX_DELAY)) {
            gVehicleData.obdConnected = true;
            xSemaphoreGive(gVehicleDataMutex);
        }
        gObdReady = true;  // unblock the poller task
    }

    vTaskDelete(nullptr);
}

// ─── setup() ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5.begin(cfg);
    gGfx = &M5.Display;
    gGfx->setRotation(1);
    Serial.printf("[MAIN] Display: %dx%d\n", gGfx->width(), gGfx->height());

    dashboard_init(gGfx);

    pidPollerStart(elm);
    xTaskCreatePinnedToCore(obd_task, "obd_task", 4096, nullptr, 1, nullptr, 0);

    Serial.println("[MAIN] Setup complete.");
}

// ─── loop() ───────────────────────────────────────────────────────────────────
void loop() {
    M5.update();
    dashboard_update();
    delay(33);  // ~30 fps
}
