#pragma once
#include <M5GFX.h>

// ─── Cyberpunk dark palette ───────────────────────────────────────────────────
// All backgrounds are neutral dark greys — zero blue or color tint.
// Accent colors appear ONLY on borders, text, icons, and small indicators.
#define COLOR_BG              0x0A0A0AU
#define COLOR_TILE_BG         0x141414U
#define COLOR_TILE_BORDER     0x282828U
#define COLOR_ACCENT          0x00FFCCU
#define COLOR_ACCENT2         0xE8FF00U
#define COLOR_TEXT_PRIMARY    0xE8E8E8U
#define COLOR_TEXT_SECONDARY  0x606060U
#define COLOR_OK              0x00C875U
#define COLOR_WARN            0xFFAA00U
#define COLOR_CRIT            0xFF2255U

// ─── Threshold helper ─────────────────────────────────────────────────────────
inline uint32_t theme_threshold_color(float value, float warnThresh, float critThresh) {
    if (value >= critThresh) return COLOR_CRIT;
    if (value >= warnThresh) return COLOR_WARN;
    return COLOR_OK;
}

// ─── Smooth anti-aliased fonts (M5GFX DejaVu) ────────────────────────────────
// Call font_*() on any LovyanGFX sprite/display before drawString().
// These replace the old setTextSize() multiplier approach (which was blocky).
template<typename T> inline void font_tiny  (T* s) { s->setFont(&lgfx::fonts::DejaVu9);  s->setTextSize(1); }
template<typename T> inline void font_small (T* s) { s->setFont(&lgfx::fonts::DejaVu18); s->setTextSize(1); }
template<typename T> inline void font_medium(T* s) { s->setFont(&lgfx::fonts::DejaVu24); s->setTextSize(1); }
template<typename T> inline void font_large (T* s) { s->setFont(&lgfx::fonts::DejaVu40); s->setTextSize(1); }
template<typename T> inline void font_xlarge(T* s) { s->setFont(&lgfx::fonts::DejaVu56); s->setTextSize(1); }

// Legacy size constants — intentionally kept at 1 so any stray setTextSize()
// calls don't produce giant scaled glyphs. Migrate to font_*() helpers instead.
#define FONT_LARGE_SIZE   1
#define FONT_MEDIUM_SIZE  1
#define FONT_SMALL_SIZE   1
