#pragma once
#include <M5GFX.h>

// Draw the dashboard into an existing sprite (provided by ui_manager).
// Returns true if anything was redrawn (vehicle data changed), false if the
// frame was identical to the last render and the sprite was not touched.
// Pass force=true to unconditionally redraw (e.g. after a tab switch).
bool dashboard_draw(LGFX_Sprite* content, bool force = false);
