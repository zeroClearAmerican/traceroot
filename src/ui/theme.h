#pragma once
#include <M5GFX.h>

// ─── Dark automotive color palette (RGB888 → M5GFX color32_t) ────────────────
#define COLOR_BG              0x0D0D0D
#define COLOR_TILE_BG         0x1A1A2E
#define COLOR_TILE_BORDER     0x16213E
#define COLOR_ACCENT          0x00D4FF
#define COLOR_TEXT_PRIMARY    0xFFFFFF
#define COLOR_TEXT_SECONDARY  0xA0A0B0
#define COLOR_OK              0x00E676
#define COLOR_WARN            0xFFD600
#define COLOR_CRIT            0xFF1744

// ─── Thresholds ───────────────────────────────────────────────────────────────
inline uint32_t theme_threshold_color(float value, float warnThresh, float critThresh) {
    if (value >= critThresh) return COLOR_CRIT;
    if (value >= warnThresh) return COLOR_WARN;
    return COLOR_OK;
}

// ─── Font sizes (M5GFX textSize multiplier, 1=8px base) ──────────────────────
#define FONT_LARGE_SIZE   6
#define FONT_MEDIUM_SIZE  3
#define FONT_SMALL_SIZE   2
