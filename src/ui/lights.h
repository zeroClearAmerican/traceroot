#pragma once
#include <M5GFX.h>

// Draw the lights control page into an existing sprite (provided by ui_manager).
// Returns true if anything was redrawn, false if the sprite was left untouched.
// Pass force=true to unconditionally redraw (e.g. after a tab switch).
bool lights_draw(LGFX_Sprite* content, bool force = false);

// Called by ui_manager when a tap lands in the content area while this tab is active.
void lights_handle_tap(int32_t x, int32_t y);
