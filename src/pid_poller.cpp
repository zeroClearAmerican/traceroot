#include "pid_poller.h"
#include "elm327.h"
#include "config.h"
#include <Arduino.h>

VehicleData gVehicleData = {};
SemaphoreHandle_t gVehicleDataMutex = nullptr;

// ─── PID list ─────────────────────────────────────────────────────────────────
static const uint8_t kPidList[] = {
    PID_ENGINE_RPM,
    PID_VEHICLE_SPEED,
    PID_COOLANT_TEMP,
    PID_ENGINE_LOAD,
    PID_THROTTLE_POS,
    PID_INTAKE_AIR_TEMP,
    PID_OIL_TEMP,
    PID_CTRL_MODULE_VOLTAGE,
};
static const size_t kPidCount = sizeof(kPidList) / sizeof(kPidList[0]);

// ─── PID decode helpers ───────────────────────────────────────────────────────
static float decodePid(uint8_t pid, uint8_t A, uint8_t B) {
    switch (pid) {
        case PID_ENGINE_RPM:            return ((A * 256.0f) + B) / 4.0f;
        case PID_VEHICLE_SPEED:         return (float)A;
        case PID_COOLANT_TEMP:          return (float)A - 40.0f;
        case PID_ENGINE_LOAD:           return A * 100.0f / 255.0f;
        case PID_THROTTLE_POS:          return A * 100.0f / 255.0f;
        case PID_INTAKE_AIR_TEMP:       return (float)A - 40.0f;
        case PID_OIL_TEMP:             return (float)A - 40.0f;
        case PID_CTRL_MODULE_VOLTAGE:   return ((A * 256.0f) + B) / 1000.0f;
        default:                        return 0.0f;
    }
}

static void applyPid(uint8_t pid, float value) {
    switch (pid) {
        case PID_ENGINE_RPM:            gVehicleData.rpm = value;               break;
        case PID_VEHICLE_SPEED:         gVehicleData.speedKmh = value;          break;
        case PID_COOLANT_TEMP:          gVehicleData.coolantTempC = value;      break;
        case PID_ENGINE_LOAD:           gVehicleData.engineLoadPct = value;     break;
        case PID_THROTTLE_POS:          gVehicleData.throttlePosPct = value;    break;
        case PID_INTAKE_AIR_TEMP:       gVehicleData.intakeAirTempC = value;    break;
        case PID_OIL_TEMP:             gVehicleData.oilTempC = value;           break;
        case PID_CTRL_MODULE_VOLTAGE:   gVehicleData.ctrlModuleVoltage = value; break;
    }
}

// ─── FreeRTOS polling task ────────────────────────────────────────────────────
static ELM327Client* gElm = nullptr;

static void pidPollerTask(void* param) {
    ELM327Client& elm = *gElm;
    size_t pidIndex = 0;

    // Wait for BT connection
    while (!elm.isConnected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // ELM327 init sequence
    elm.init();

    if (xSemaphoreTake(gVehicleDataMutex, portMAX_DELAY)) {
        gVehicleData.obdConnected = true;
        String proto = elm.getProtocol();
        proto.toCharArray(gVehicleData.obdProtocol, sizeof(gVehicleData.obdProtocol));
        xSemaphoreGive(gVehicleDataMutex);
    }

    for (;;) {
        uint8_t pid = kPidList[pidIndex];
        uint8_t A = 0, B = 0;

        if (elm.queryPID(pid, A, B)) {
            float val = decodePid(pid, A, B);
            if (xSemaphoreTake(gVehicleDataMutex, pdMS_TO_TICKS(10))) {
                applyPid(pid, val);
                xSemaphoreGive(gVehicleDataMutex);
            }
        }

        pidIndex = (pidIndex + 1) % kPidCount;
        vTaskDelay(pdMS_TO_TICKS(PID_POLL_INTERVAL_MS));
    }
}

void pidPollerStart(ELM327Client& elm) {
    gElm = &elm;
    gVehicleDataMutex = xSemaphoreCreateMutex();
    // Pin to core 0; LVGL runs on core 1
    xTaskCreatePinnedToCore(pidPollerTask, "pid_poller", 4096, nullptr, 2, nullptr, 0);
}
