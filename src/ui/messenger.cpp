// messenger.cpp — LoRa P2P messenger page
// Data-driven from gLoraData; dirty-checked via memcmp snapshot.

#include "messenger.h"
#include "theme.h"
#include "../lora_manager.h"
#include "../config.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <string.h>

// ─── Layout constants ─────────────────────────────────────────────────────────
static const int32_t W             = DISPLAY_WIDTH  - 72;   // content area width
static const int32_t H             = DISPLAY_HEIGHT;         // content area height

static const int32_t HEADER_H      = 56;   // status/connection bar
static const int32_t FOOTER_H      = 72;   // send button row
static const int32_t SIDEBAR_W     = 220;  // right-side settings panel (hidden unless toggled)

static const int32_t MSG_AREA_Y    = HEADER_H;
static const int32_t MSG_AREA_H    = H - HEADER_H - FOOTER_H;
static const int32_t MSG_AREA_X    = 0;
static const int32_t MSG_AREA_W    = W;

static const int32_t MAX_VISIBLE   = 10;   // max message bubbles shown

// Send button bounds (in content coordinates)
static const int32_t BTN_SEND_X    = W - 200;
static const int32_t BTN_SEND_Y    = H - FOOTER_H + 12;
static const int32_t BTN_SEND_W    = 180;
static const int32_t BTN_SEND_H    = 48;

// ─── Persistent state ─────────────────────────────────────────────────────────
static LoraData sLastData;   // previous snapshot for dirty check
static bool     sFirstDraw = true;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t state_color(LoraState s) {
    switch (s) {
        case LORA_STATE_READY:      return COLOR_OK;
        case LORA_STATE_CONNECTING: return COLOR_WARN;
        default:                    return COLOR_CRIT;
    }
}

// Draw a single message bubble.
// isMine=true → right-aligned teal; isMine=false → left-aligned tile bubble.
static void draw_bubble(LGFX_Sprite* c, int32_t y, const LoraMessage& msg) {
    const int32_t PAD   = 16;
    const int32_t MAX_W = MSG_AREA_W - 80;
    const int32_t BH    = 44;  // bubble height (single-line messages)

    if (msg.isMine) {
        // Right-aligned — neon teal bubble
        int32_t bx = MSG_AREA_W - MAX_W - PAD;
        c->fillRoundRect(bx, y, MAX_W, BH, 10, COLOR_TILE_BG);
        c->drawRoundRect(bx, y, MAX_W, BH, 10, COLOR_ACCENT);
        font_small(c);
        c->setTextColor(COLOR_TEXT_PRIMARY);
        c->setTextDatum(textdatum_t::middle_left);
        c->drawString(msg.text, bx + 12, y + BH / 2);
        // "YOU" label
        font_tiny(c);
        c->setTextColor(COLOR_TEXT_SECONDARY);
        c->setTextDatum(textdatum_t::bottom_right);
        c->drawString("YOU", MSG_AREA_W - PAD, y);
    } else {
        // Left-aligned — dark tile bubble
        int32_t bx = PAD;
        c->fillRoundRect(bx, y, MAX_W, BH, 10, COLOR_TILE_BG);
        c->drawRoundRect(bx, y, MAX_W, BH, 10, COLOR_TILE_BORDER);
        font_small(c);
        c->setTextColor(COLOR_TEXT_PRIMARY);
        c->setTextDatum(textdatum_t::middle_left);
        c->drawString(msg.text, bx + 12, y + BH / 2);
        // RSSI / SNR annotation
        char meta[24];
        snprintf(meta, sizeof(meta), "%d dBm / %d dB", (int)msg.rssi, (int)msg.snr);
        font_tiny(c);
        c->setTextColor(COLOR_TEXT_SECONDARY);
        c->setTextDatum(textdatum_t::bottom_left);
        c->drawString(meta, bx, y);
    }
}

// ─── Public: draw ─────────────────────────────────────────────────────────────

bool messenger_draw(LGFX_Sprite* content, bool force) {
    // Take a safe snapshot of shared LoRa state
    LoraData snap;
    if (xSemaphoreTake(gLoraMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        snap = gLoraData;
        xSemaphoreGive(gLoraMutex);
    } else {
        // Couldn't grab mutex — keep last snapshot, skip draw if not forced
        if (!force && !sFirstDraw) return false;
        snap = sLastData;
    }

    // Dirty check
    bool dirty = force || sFirstDraw ||
                 (memcmp(&snap, &sLastData, sizeof(LoraData)) != 0);
    if (!dirty) return false;

    sLastData  = snap;
    sFirstDraw = false;

    // ── Clear background ──────────────────────────────────────────────────────
    content->fillSprite(COLOR_BG);

    // ── Header / status bar ───────────────────────────────────────────────────
    uint32_t sc = state_color(snap.state);
    content->fillRect(0, 0, W, HEADER_H, COLOR_TILE_BG);
    content->drawFastHLine(0, HEADER_H - 1, W, COLOR_TILE_BORDER);

    // Status dot
    content->fillCircle(24, HEADER_H / 2, 7, sc);
    content->drawCircle(24, HEADER_H / 2, 9, sc);  // glow ring

    // State string
    font_small(content);
    content->setTextColor(sc);
    content->setTextDatum(textdatum_t::middle_left);
    content->drawString(snap.stateStr, 44, HEADER_H / 2);

    // RSSI readout (if connected)
    if (snap.state == LORA_STATE_READY && snap.msgCount > 0) {
        char rssiStr[32];
        snprintf(rssiStr, sizeof(rssiStr), "RSSI %d dBm", (int)snap.lastRssi);
        font_tiny(content);
        content->setTextColor(COLOR_TEXT_SECONDARY);
        content->setTextDatum(textdatum_t::middle_right);
        content->drawString(rssiStr, W - 16, HEADER_H / 2);
    }

    // ── Message list ─────────────────────────────────────────────────────────
    // Show newest messages at the bottom (walk ring buffer in reverse)
    // Collect up to MAX_VISIBLE indices
    int indices[MAX_VISIBLE];
    int count = 0;
    int total = min(snap.msgCount, LORA_MAX_MESSAGES);
    for (int i = total - 1; i >= 0 && count < MAX_VISIBLE; i--) {
        indices[count++] = i;
    }
    // indices[0] = newest; draw them bottom-up
    const int32_t BUBBLE_H    = 44;
    const int32_t BUBBLE_GAP  = 12;
    const int32_t BUBBLE_STEP = BUBBLE_H + BUBBLE_GAP + 14; // 14 for meta text above

    for (int di = 0; di < count; di++) {
        int32_t y = MSG_AREA_Y + MSG_AREA_H - BUBBLE_H - (di * BUBBLE_STEP) - 8;
        if (y < MSG_AREA_Y) break;
        draw_bubble(content, y, snap.messages[indices[di]]);
    }

    // Empty-state hint
    if (snap.msgCount == 0) {
        font_small(content);
        content->setTextColor(COLOR_TEXT_SECONDARY);
        content->setTextDatum(textdatum_t::middle_center);
        const char* hint = (snap.state == LORA_STATE_READY)
            ? "No messages yet"
            : "Waiting for LoRa module...";
        content->drawString(hint, W / 2, MSG_AREA_Y + MSG_AREA_H / 2);
    }

    // ── Footer / send row ────────────────────────────────────────────────────
    int32_t footer_y = H - FOOTER_H;
    content->fillRect(0, footer_y, W, FOOTER_H, COLOR_TILE_BG);
    content->drawFastHLine(0, footer_y, W, COLOR_TILE_BORDER);

    // Channel / freq label on the left
    char freqLabel[40];
    snprintf(freqLabel, sizeof(freqLabel), "P2P  %.3f MHz  SF%d",
             LORA_FREQ_HZ / 1e6f, (int)LORA_SF);
    font_tiny(content);
    content->setTextColor(COLOR_TEXT_SECONDARY);
    content->setTextDatum(textdatum_t::middle_left);
    content->drawString(freqLabel, 16, footer_y + FOOTER_H / 2);

    // Send button
    bool canSend = (snap.state == LORA_STATE_READY);
    uint32_t btnFill   = COLOR_TILE_BG;
    uint32_t btnBorder = canSend ? (uint32_t)COLOR_ACCENT : (uint32_t)COLOR_TEXT_SECONDARY;
    uint32_t btnText   = canSend ? (uint32_t)COLOR_ACCENT : (uint32_t)COLOR_TEXT_SECONDARY;
    content->fillRoundRect(BTN_SEND_X, BTN_SEND_Y, BTN_SEND_W, BTN_SEND_H, 10, btnFill);
    content->drawRoundRect(BTN_SEND_X, BTN_SEND_Y, BTN_SEND_W, BTN_SEND_H, 10, btnBorder);
    font_small(content);
    content->setTextColor(btnText);
    content->setTextDatum(textdatum_t::middle_center);
    content->drawString("SEND TEST", BTN_SEND_X + BTN_SEND_W / 2,
                        BTN_SEND_Y + BTN_SEND_H / 2);

    return true;
}

// ─── Public: tap handler ──────────────────────────────────────────────────────

void messenger_handle_tap(int32_t x, int32_t y) {
    // Send button
    if (x >= BTN_SEND_X && x <= BTN_SEND_X + BTN_SEND_W &&
        y >= BTN_SEND_Y && y <= BTN_SEND_Y + BTN_SEND_H) {
        bool ok = loraSend("TEST");
        Serial.printf("[MSG] Send TAP → loraSend returned %s\n", ok ? "OK" : "FAIL");
    }
}
