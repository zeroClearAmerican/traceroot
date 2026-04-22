#pragma once
#include <Arduino.h>
#include <ELMduino.h>

// ─── Shared vehicle data ──────────────────────────────────────────────────────
struct VehicleData {
    float   rpm;
    float   speedKmh;
    float   coolantTempC;
    float   engineLoadPct;
    float   throttlePosPct;
    float   intakeAirTempC;
    float   oilTempC;
    float   ctrlModuleVoltage;

    bool    btConnected;
    bool    obdConnected;
    char    obdProtocol[16];
};

extern VehicleData        gVehicleData;
extern SemaphoreHandle_t  gVehicleDataMutex;
extern volatile bool      gObdReady;  // set to true after elm.begin() succeeds

// Start the FreeRTOS polling task. Pass a fully initialized ELM327 instance.
void pidPollerStart(ELM327& elm);
