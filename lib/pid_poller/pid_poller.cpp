#include "pid_poller.h"
#include "../../src/config.h"
#include <ELMduino.h>

VehicleData       gVehicleData      = {};
SemaphoreHandle_t gVehicleDataMutex = nullptr;
volatile bool     gObdReady         = false;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static inline void writeFloat(float& field, float value) {
    if (xSemaphoreTake(gVehicleDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        field = value;
        xSemaphoreGive(gVehicleDataMutex);
    }
}

// ─── Polling states ───────────────────────────────────────────────────────────
enum PollState : uint8_t {
    POLL_RPM = 0,
    POLL_SPEED,
    POLL_COOLANT,
    POLL_LOAD,
    POLL_THROTTLE,
    POLL_INTAKE,
    POLL_OIL,
    POLL_VOLTAGE,
    POLL_COUNT
};

// ─── FreeRTOS task ────────────────────────────────────────────────────────────
static ELM327* gElm = nullptr;

static void pidPollerTask(void* param) {
    // Wait until elm.begin() has been called successfully
    while (!gObdReady) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ELM327& elm = *gElm;
    PollState state = POLL_RPM;

    for (;;) {
        float raw = 0.0f;
        bool  done = false;

        switch (state) {
            case POLL_RPM:      raw = elm.rpm();                          break;
            case POLL_SPEED:    raw = elm.kph();                          break;
            case POLL_COOLANT:  raw = elm.engineCoolantTemp();            break;
            case POLL_LOAD:     raw = elm.engineLoad();                   break;
            case POLL_THROTTLE: raw = elm.throttle();                     break;
            case POLL_INTAKE:   raw = elm.intakeAirTemp();                break;
            case POLL_OIL:      raw = elm.oilTemp();                      break;
            case POLL_VOLTAGE:  raw = elm.batteryVoltage();               break;
            default:            state = POLL_RPM; continue;
        }

        uint8_t nb = elm.nb_rx_state;

        if (nb == ELM_SUCCESS) {
            if (xSemaphoreTake(gVehicleDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                switch (state) {
                    case POLL_RPM:      gVehicleData.rpm               = raw; break;
                    case POLL_SPEED:    gVehicleData.speedKmh           = raw; break;
                    case POLL_COOLANT:  gVehicleData.coolantTempC       = raw; break;
                    case POLL_LOAD:     gVehicleData.engineLoadPct      = raw; break;
                    case POLL_THROTTLE: gVehicleData.throttlePosPct     = raw; break;
                    case POLL_INTAKE:   gVehicleData.intakeAirTempC     = raw; break;
                    case POLL_OIL:      gVehicleData.oilTempC           = raw; break;
                    case POLL_VOLTAGE:  gVehicleData.ctrlModuleVoltage  = raw; break;
                    default: break;
                }
                xSemaphoreGive(gVehicleDataMutex);
            }
            state = (PollState)((state + 1) % POLL_COUNT);
        } else if (nb != ELM_GETTING_MSG) {
            // Error on this PID — skip to next
            Serial.printf("[POLLER] PID %d error: %d\n", (int)state, (int)nb);
            state = (PollState)((state + 1) % POLL_COUNT);
        }

        vTaskDelay(pdMS_TO_TICKS(PID_POLL_INTERVAL_MS));
    }
}

void pidPollerStart(ELM327& elm) {
    gElm = &elm;
    gVehicleDataMutex = xSemaphoreCreateMutex();
    // Pin to core 0; LVGL renders on core 1 (Arduino loop)
    xTaskCreatePinnedToCore(pidPollerTask, "pid_poller", 4096, nullptr, 2, nullptr, 0);
}
