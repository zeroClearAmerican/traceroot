#include "ui_manager.h"
#include "theme.h"
#include "dashboard.h"
#include "messenger.h"
#include "lights.h"
#include "../config.h"
#include <Arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>

// ─── Layout ───────────────────────────────────────────────────────────────────
static const int32_t TABBAR_W   = 72;   // left sidebar width
static const int32_t SCREEN_W   = DISPLAY_WIDTH;
static const int32_t SCREEN_H   = DISPLAY_HEIGHT;
static const int32_t CONTENT_W  = SCREEN_W - TABBAR_W;
static const int32_t CONTENT_H  = SCREEN_H;
// Each tab gets an equal slice of the full bar height
static const int32_t NUM_TABS   = 3;
static const int32_t TAB_H      = SCREEN_H / NUM_TABS;

// ─── State ────────────────────────────────────────────────────────────────────
static M5GFX*      gGfx         = nullptr;
static LGFX_Sprite* gScreen     = nullptr;  // full-screen composite sprite
static LGFX_Sprite* gContent    = nullptr;  // content area sub-sprite

static int  gActiveTab     = 0;
static int  gNumTabs       = NUM_TABS;
static int  gLastActiveTab = -1;  // tracks tab switches to force-redraw on change

static bool gLastTouched       = false;
static int32_t gLastTouchX     = 0;
static int32_t gLastTouchY     = 0;

// ─── Tab definitions ──────────────────────────────────────────────────────────
struct TabDef {
    const char* label;
    // icon: drawn procedurally
    void (*drawFn)(LGFX_Sprite*);
};

// Forward-declare icon draw helpers
static void icon_obd     (LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color);
static void icon_lora    (LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color);
static void icon_lights  (LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color);

static const TabDef TABS[] = {
    { "OBD",   nullptr },
    { "MSG",   nullptr },
    { "LTS",   nullptr },
};

// ─── Icon drawing helpers ─────────────────────────────────────────────────────

// OBD icon: speedometer needle + arc
static void icon_obd(LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color) {
    s->drawArc(cx, cy, 18, 15, 210, 330, color);
    // needle pointing upper-right
    s->drawLine(cx, cy, cx + 11, cy - 10, color);
    s->fillCircle(cx, cy, 3, color);
    // small tick marks
    for (int a = 210; a <= 330; a += 30) {
        float rad = a * 3.14159f / 180.0f;
        int x1 = cx + (int)(18 * cosf(rad));
        int y1 = cy + (int)(18 * sinf(rad));
        int x2 = cx + (int)(22 * cosf(rad));
        int y2 = cy + (int)(22 * sinf(rad));
        s->drawLine(x1, y1, x2, y2, color);
    }
}

// LoRa icon: upward-radiating arcs + base dot
static void icon_lora(LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color) {
    s->fillCircle(cx, cy + 6, 4, color);
    s->drawArc(cx, cy + 6,  10,  8, 225, 315, color);
    s->drawArc(cx, cy + 6,  17, 15, 220, 320, color);
    s->drawArc(cx, cy + 6,  24, 22, 215, 325, color);
}

// Lights icon: central bulb circle + four radiating rays
static void icon_lights(LGFX_Sprite* s, int32_t cx, int32_t cy, uint32_t color) {
    s->drawCircle(cx, cy, 8, color);
    s->fillCircle(cx, cy, 4, color);
    // Cardinal rays
    for (int a = 0; a < 360; a += 45) {
        float rad = a * 3.14159f / 180.0f;
        int x1 = cx + (int)(11 * cosf(rad));
        int y1 = cy + (int)(11 * sinf(rad));
        int x2 = cx + (int)(17 * cosf(rad));
        int y2 = cy + (int)(17 * sinf(rad));
        s->drawLine(x1, y1, x2, y2, color);
    }
}

// ─── Tab bar draw ─────────────────────────────────────────────────────────────

static void draw_tabbar(LGFX_Sprite* s) {
    // Background
    s->fillRect(0, 0, TABBAR_W, SCREEN_H, COLOR_BG);
    s->drawFastVLine(TABBAR_W - 1, 0, SCREEN_H, COLOR_TILE_BORDER);

    for (int i = 0; i < gNumTabs; i++) {
        int32_t ty   = i * TAB_H;
        int32_t cx   = TABBAR_W / 2;
        int32_t icoy = ty + TAB_H / 2 - 20;
        int32_t lbly = ty + TAB_H / 2 + 14;

        bool active = (i == gActiveTab);
        uint32_t tab_bg = active ? COLOR_TILE_BG : COLOR_BG;
        uint32_t icon_c = active ? COLOR_ACCENT : COLOR_TEXT_SECONDARY;

        s->fillRect(1, ty, TABBAR_W - 2, TAB_H, tab_bg);

        if (active) {
            // Active neon teal bar on right edge
            s->fillRect(TABBAR_W - 3, ty + 10, 3, TAB_H - 20, COLOR_ACCENT);
        }

        // Draw icon
        if (i == 0) icon_obd   (s, cx, icoy + 12, icon_c);
        if (i == 1) icon_lora  (s, cx, icoy + 6,  icon_c);
        if (i == 2) icon_lights(s, cx, icoy + 12, icon_c);

        // Label — tiny smooth font
        font_tiny(s);
        s->setTextColor(icon_c);
        s->setTextDatum(textdatum_t::top_center);
        s->drawString(TABS[i].label, cx, lbly);

        // Divider
        if (i < gNumTabs - 1) {
            s->drawFastHLine(8, ty + TAB_H - 1, TABBAR_W - 16, COLOR_TILE_BORDER);
        }
    }
}

// ─── Touch handling ───────────────────────────────────────────────────────────

static void handle_touch() {
    auto tp = M5.Touch.getDetail();
    bool pressed = tp.isPressed();

    if (pressed && !gLastTouched) {
        // New press — log coordinates every time
        gLastTouchX = tp.x;
        gLastTouchY = tp.y;
        // Serial.printf("[TOUCH] DOWN x=%d y=%d  tabbar_w=%d tab_h=%d\n",
        //               gLastTouchX, gLastTouchY, (int)TABBAR_W, (int)TAB_H);

        // Is it in the tab bar?
        if (gLastTouchX < TABBAR_W) {
            bool hit = false;
            for (int i = 0; i < gNumTabs; i++) {
                int32_t ty = i * TAB_H;
                if (gLastTouchY >= ty && gLastTouchY < ty + TAB_H) {
                    Serial.printf("[TOUCH] Tab %d selected (ty=%d..%d)\n",
                                  i, (int)ty, (int)(ty + TAB_H));
                    gActiveTab = i;
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                // Serial.println("[TOUCH] In tabbar X range but no tab row matched");
            }
        } else {
            // Serial.println("[TOUCH] Content area (not tab bar)");
            // Forward content-area taps to the active page
            int32_t content_x = gLastTouchX - TABBAR_W;
            int32_t content_y = gLastTouchY;
            if (gActiveTab == 1) {
                messenger_handle_tap(content_x, content_y);
            } else if (gActiveTab == 2) {
                lights_handle_tap(content_x, content_y);
            }
        }
    }

    if (!pressed && gLastTouched) {
        // Serial.printf("[TOUCH] UP   x=%d y=%d\n", (int)tp.x, (int)tp.y);
    }

    gLastTouched = pressed;
}

// ─── Public API ───────────────────────────────────────────────────────────────

void ui_init(M5GFX* gfx) {
    gGfx = gfx;

    // Full-screen composite sprite (in PSRAM)
    gScreen = new LGFX_Sprite(gfx);
    gScreen->setColorDepth(16);
    gScreen->setPsram(true);
    if (!gScreen->createSprite(SCREEN_W, SCREEN_H)) {
        Serial.println("[UI] Screen sprite alloc FAILED");
        delete gScreen;
        gScreen = nullptr;
        return;
    }
    Serial.printf("[UI] Screen sprite OK (%dx%d)\n", SCREEN_W, SCREEN_H);

    // Content sub-sprite (draws into gScreen's buffer, offset by TABBAR_W)
    gContent = new LGFX_Sprite(gScreen);
    gContent->setColorDepth(16);
    // No PSRAM needed — this is a view into the parent sprite's memory region
    if (!gContent->createSprite(CONTENT_W, CONTENT_H)) {
        Serial.println("[UI] Content sprite alloc FAILED — falling back to PSRAM");
        gContent->setPsram(true);
        if (!gContent->createSprite(CONTENT_W, CONTENT_H)) {
            Serial.println("[UI] Content sprite PSRAM alloc also FAILED");
            delete gContent;
            gContent = nullptr;
        }
    }
    Serial.printf("[UI] Content sprite OK (%dx%d)\n", CONTENT_W, CONTENT_H);
}

void ui_update() {
    if (!gScreen || !gContent) return;

    // Always poll input — never rate-limited
    M5.update();
    handle_touch();

    // Gate rendering to ~30 fps so slow sprite push doesn't starve touch
    static uint32_t sLastRender = 0;
    uint32_t now = millis();
    if (now - sLastRender < 33) return;
    sLastRender = now;

    bool tabSwitched = (gActiveTab != gLastActiveTab);
    if (tabSwitched) gLastActiveTab = gActiveTab;

    // Draw active page into content sprite; returns true if anything changed
    bool contentDirty = false;
    switch (gActiveTab) {
        case 0: contentDirty = dashboard_draw(gContent, tabSwitched); break;
        case 1: contentDirty = messenger_draw(gContent, tabSwitched); break;
        case 2: contentDirty = lights_draw   (gContent, tabSwitched); break;
        default: break;
    }

    // Redraw tab bar only when the active tab changed
    if (tabSwitched) {
        draw_tabbar(gScreen);
    }

    // Only push to display if something actually changed
    if (contentDirty || tabSwitched) {
        // Blit content sprite into screen sprite at X=TABBAR_W
        gContent->pushSprite(gScreen, TABBAR_W, 0);
        // Push final composite to display
        gScreen->pushSprite(0, 0);
    }
}
