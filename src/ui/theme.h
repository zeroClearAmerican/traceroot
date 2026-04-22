#pragma once

// ─── Dark automotive color palette ───────────────────────────────────────────
#define COLOR_BG            0x0D0D0D   // near-black background
#define COLOR_TILE_BG       0x1A1A2E   // tile background (dark navy)
#define COLOR_TILE_BORDER   0x16213E
#define COLOR_ACCENT        0x00D4FF   // cyan accent / arc stroke
#define COLOR_TEXT_PRIMARY  0xFFFFFF
#define COLOR_TEXT_SECONDARY 0xA0A0B0

// Thresholds color coding
#define COLOR_OK            0x00E676   // green
#define COLOR_WARN          0xFFD600   // amber
#define COLOR_CRIT          0xFF1744   // red

// ─── LVGL color helpers ───────────────────────────────────────────────────────
#include <lvgl.h>

inline lv_color_t theme_color(uint32_t hex) {
    return lv_color_make((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline lv_color_t theme_threshold_color(float value, float warnThresh, float critThresh) {
    if (value >= critThresh) return theme_color(COLOR_CRIT);
    if (value >= warnThresh) return theme_color(COLOR_WARN);
    return theme_color(COLOR_OK);
}

// ─── Fonts ────────────────────────────────────────────────────────────────────
// Using LVGL built-in fonts; swap for custom fonts once available
#define FONT_LARGE   &lv_font_montserrat_48
#define FONT_MEDIUM  &lv_font_montserrat_24
#define FONT_SMALL   &lv_font_montserrat_14
