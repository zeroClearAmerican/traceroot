#pragma once
#include <Arduino.h>

// ─── Vehicle Data ─────────────────────────────────────────────────────────────
struct VehicleData {
    float   rpm;                   // engine RPM
    float   speedKmh;              // vehicle speed in km/h
    float   coolantTempC;          // coolant temperature °C
    float   engineLoadPct;         // engine load %
    float   throttlePosPct;        // throttle position %
    float   intakeAirTempC;        // intake air temperature °C
    float   oilTempC;              // oil temperature °C
    float   ctrlModuleVoltage;     // control module voltage V

    bool    btConnected;           // Bluetooth link alive
    bool    obdConnected;          // ELM327 handshake complete
    char    obdProtocol[16];       // protocol string from ATDP response
};

extern VehicleData gVehicleData;
extern SemaphoreHandle_t gVehicleDataMutex;
