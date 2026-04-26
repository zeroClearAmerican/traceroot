#include "lights.h"
#include "theme.h"
#include "../config.h"
#include <M5GFX.h>
#include <Arduino.h>
#include <cstring>

// ─── Light channel definitions ────────────────────────────────────────────────

enum LightMode : uint8_t {
    MODE_OFF    = 0,
    MODE_ON     = 1,
    MODE_AMBER  = 2,   // windshield bar only
};

struct LightChannel {
    const char* name;
    const char* subtitle;       // second line of label, e.g. "Primary / Amber"
    bool        hasAmber;       // whether this channel has a third amber state
    LightMode   mode;
};

static LightChannel sChannels[] = {
    { "WINDSHIELD",  "Lightbar",       true,  MODE_OFF },
    { "DITCH",       "Windshield",     false, MODE_OFF },
    { "CAMP LEFT",   "Side light",     false, MODE_OFF },
    { "CAMP RIGHT",  "Side light",     false, MODE_OFF },
};
static const int NUM_CHANNELS = 4;

// Track last rendered state so we only redraw on change
static LightChannel sLastRendered[NUM_CHANNELS];
static bool         sHasRendered = false;
static bool         sDirty       = false;   // set by lights_handle_tap

// ─── Layout ───────────────────────────────────────────────────────────────────
static const int32_t PAD         = 20;
static const int32_t CARD_R      = 16;   // corner radius
// Cards laid out in a 2×2 grid with a full-width header bar
static const int32_t HEADER_H    = 64;
// Computed at draw time from sprite dimensions

// ─── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t mode_color(LightMode m) {
    switch (m) {
        case MODE_ON:    return COLOR_ACCENT;    // neon teal
        case MODE_AMBER: return COLOR_WARN;      // neon amber
        default:         return COLOR_TEXT_SECONDARY;
    }
}

static const char* mode_label(LightMode m) {
    switch (m) {
        case MODE_ON:    return "ON";
        case MODE_AMBER: return "AMBER";
        default:         return "OFF";
    }
}

// Draw a power-button icon at (cx,cy)
static void draw_power_icon(LGFX_Sprite* s, int32_t cx, int32_t cy, int32_t r, uint32_t color) {
    s->drawArc(cx, cy, r, r - 3, 120, 420, color);
    s->drawFastVLine(cx, cy - r, r / 2 + 2, color);
    s->drawFastVLine(cx, cy - r + 1, r / 2 + 2, color); // 2px wide
}

// Draw a light-beam icon (two diverging rays)
static void draw_beam_icon(LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color) {
    // Source dot
    s->fillCircle(cx, cy, 4, color);
    // Three rays fanning right
    for (int angle = -30; angle <= 30; angle += 30) {
        float rad = angle * 3.14159f / 180.0f;
        int ex = cx + (int)(28 * cosf(rad));
        int ey = cy + (int)(28 * sinf(rad));
        s->drawLine(cx + 5, cy, ex, ey, color);
    }
}

// Proportional layout helpers
static int32_t prop(int32_t base, int32_t ref, int32_t val) {
    // Scale val (designed for base) to ref
    return (val * ref) / base;
}

// Draw static card background (fill, border, icon, labels)
static void draw_card_bg(LGFX_Sprite* s, int32_t x, int32_t y, int32_t w, int32_t h, const LightChannel& ch, int idx) {
    uint32_t border_c = mode_color(ch.mode);
    s->fillRoundRect(x, y, w, h, prop(240, w, CARD_R), COLOR_TILE_BG);
    s->drawRoundRect(x, y, w, h, prop(240, w, CARD_R), border_c);
    int32_t cx = x + w / 2;
    // Icon
    int32_t icon_y = y + prop(240, h, 32);
    int32_t icon_r = prop(240, w, 18);
    if (ch.mode != MODE_OFF) {
        draw_beam_icon(s, cx, icon_y, border_c);
    } else {
        draw_power_icon(s, cx, icon_y, icon_r, COLOR_TEXT_SECONDARY);
    }
    // Channel name
    s->setTextDatum(textdatum_t::top_center);
    font_large(s); // larger font for card title
    s->setTextColor(ch.mode != MODE_OFF ? (uint32_t)COLOR_TEXT_PRIMARY : (uint32_t)COLOR_TEXT_SECONDARY);
    s->drawString(ch.name, cx, y + prop(240, h, 70));
    font_small(s);
    s->setTextColor(COLOR_TEXT_SECONDARY);
    s->drawString(ch.subtitle, cx, y + prop(240, h, 98));
}

// Draw dynamic card foreground (badge, amber sub-button)
static void draw_card_fg(LGFX_Sprite* s, int32_t x, int32_t y, int32_t w, int32_t h, const LightChannel& ch, int idx) {
    uint32_t border_c = mode_color(ch.mode);
    int32_t cx = x + w / 2;
    // Mode badge
    int32_t badge_w = prop(240, w, 110), badge_h = prop(240, h, 32);
    int32_t badge_x = cx - badge_w / 2;
    int32_t badge_y = y + h - prop(240, h, 52);
    s->fillRoundRect(badge_x, badge_y, badge_w, badge_h, prop(240, badge_w, 8), COLOR_TILE_BG);
    s->drawRoundRect(badge_x, badge_y, badge_w, badge_h, prop(240, badge_w, 8), border_c);
    font_medium(s);
    s->setTextColor(border_c);
    s->setTextDatum(textdatum_t::middle_center);
    s->drawString(mode_label(ch.mode), cx, badge_y + badge_h / 2);
    // Amber sub-button
    if (ch.hasAmber) {
        int32_t ab_w = prop(240, w, 90), ab_h = prop(240, h, 26);
        int32_t ab_x = cx - ab_w / 2;
        int32_t ab_y = badge_y - ab_h - prop(240, h, 6);
        bool amberActive = (ch.mode == MODE_AMBER);
        s->fillRoundRect(ab_x, ab_y, ab_w, ab_h, prop(240, ab_w, 6), COLOR_TILE_BG);
        s->drawRoundRect(ab_x, ab_y, ab_w, ab_h, prop(240, ab_w, 6), amberActive ? (uint32_t)COLOR_WARN : (uint32_t)COLOR_TEXT_SECONDARY);
        font_small(s);
        s->setTextColor(amberActive ? (uint32_t)COLOR_WARN : (uint32_t)COLOR_TEXT_SECONDARY);
        s->setTextDatum(textdatum_t::middle_center);
        s->drawString("AMBER", cx, ab_y + ab_h / 2);
    }
}

// ─── Hit-test helpers ─────────────────────────────────────────────────────────
// Card bounds computed identically in draw and hit-test.

struct CardRect { int32_t x, y, w, h; };

static CardRect card_rect(int idx, int32_t W, int32_t H) {
    int32_t grid_y  = HEADER_H + PAD;
    int32_t grid_h  = H - grid_y - PAD;
    int32_t card_w  = (W - PAD * 3) / 2;
    int32_t card_h  = (grid_h - PAD) / 2;
    int32_t col     = idx % 2;
    int32_t row     = idx / 2;
    return {
        PAD + col * (card_w + PAD),
        grid_y + row * (card_h + PAD),
        card_w,
        card_h
    };
}

// ─── Public API ───────────────────────────────────────────────────────────────

void lights_handle_tap(int32_t x, int32_t y) {
    // We don't have sprite dimensions here, so use config constants
    int32_t W = DISPLAY_WIDTH  - 72;   // content width (SCREEN_W - TABBAR_W)
    int32_t H = DISPLAY_HEIGHT;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        CardRect r = card_rect(i, W, H);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            LightChannel& ch = sChannels[i];

            // Check if amber sub-button was tapped (windshield bar only)
            if (ch.hasAmber) {
                int32_t badge_y = r.y + r.h - 48;
                int32_t ab_h    = 22;
                int32_t ab_y    = badge_y - ab_h - 6;
                int32_t ab_x    = r.x + r.w / 2 - 40;
                if (x >= ab_x && x < ab_x + 80 && y >= ab_y && y < ab_y + ab_h) {
                    ch.mode = (ch.mode == MODE_AMBER) ? MODE_OFF : MODE_AMBER;
                    Serial.printf("[LIGHTS] Ch%d → AMBER toggle: %s\n", i, mode_label(ch.mode));
                    sDirty = true;
                    return;
                }
            }

            // Main card tap: cycle OFF → ON → OFF (skip AMBER, that's the sub-button)
            if (ch.mode == MODE_OFF) {
                ch.mode = MODE_ON;
            } else {
                ch.mode = MODE_OFF;
            }
            Serial.printf("[LIGHTS] Ch%d (%s) → %s\n", i, ch.name, mode_label(ch.mode));
            sDirty = true;
            return;
        }
    }
}

bool lights_draw(LGFX_Sprite* content, bool force) {
    if (!content) return false;
    if (sHasRendered && !force && !sDirty) return false;
    if (sHasRendered && !force && memcmp(sChannels, sLastRendered, sizeof(sChannels)) == 0) {
        sDirty = false;
        return false;
    }
    memcpy(sLastRendered, sChannels, sizeof(sChannels));
    sHasRendered = true;
    sDirty       = false;
    int32_t W = content->width();
    int32_t H = content->height();
    content->fillScreen(COLOR_BG);
    // ── Header ────────────────────────────────────────────────────────────────
    content->fillRect(0, 0, W, HEADER_H, COLOR_TILE_BG);
    content->drawFastHLine(0, HEADER_H - 1, W, COLOR_TILE_BORDER);
    content->setTextDatum(textdatum_t::middle_left);
    font_large(content);
    content->setTextColor(COLOR_ACCENT);
    content->drawString("LIGHTING CONTROL", PAD, HEADER_H / 2);
    // HW status pill
    content->fillRoundRect(W - 230, 18, 220, 28, 14, COLOR_TILE_BG);
    content->drawRoundRect(W - 230, 18, 220, 28, 14, COLOR_CRIT);
    font_small(content);
    content->setTextColor(COLOR_CRIT);
    content->setTextDatum(textdatum_t::middle_center);
    content->drawString("HW: Not Connected", W - 230 + 110, 18 + 14);
    // ── 2×2 card grid ─────────────────────────────────────────────────────────
    for (int i = 0; i < NUM_CHANNELS; i++) {
        CardRect r = card_rect(i, W, H);
        draw_card_bg(content, r.x, r.y, r.w, r.h, sChannels[i], i);
        draw_card_fg(content, r.x, r.y, r.w, r.h, sChannels[i], i);
    }
    return true;
}
