#pragma once
#include <M5GFX.h>

// Initialize the dashboard — must be called after M5.begin()
void dashboard_init(M5GFX* gfx);

// Refresh widget values from gVehicleData (call from loop)
void dashboard_update();
