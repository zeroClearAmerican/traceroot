#pragma once
#include <lvgl.h>

// Initialize the dashboard screen and all widgets.
void dashboard_init();

// Refresh widget values from gVehicleData (call from LVGL timer or loop).
void dashboard_update();
