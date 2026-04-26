#include "dashboard.h"
#include "theme.h"
#include <pid_poller.h>
#include "../config.h"
#include <M5GFX.h>
#include <Arduino.h>
#include <cstdio>
#include <cstring>

// ─── Layout constants ─────────────────────────────────────────────────────────
static const int32_t PAD       = 12;
static const int32_t TILE_SM   = 180;
static const int32_t ROW1_H    = 320;
static const int32_t ROW2_H    = 160;
static const int32_t STATUS_H  = 48;

// Last-rendered snapshot — used to skip identical frames
static VehicleData sLastData;
static bool        sHasRendered = false;

// ─── Drawing helpers ──────────────────────────────────────────────────────────

static void draw_tile(LGFX_Sprite* s, int32_t x, int32_t y, int32_t w, int32_t h) {
    s->fillRoundRect(x, y, w, h, 12, COLOR_TILE_BG);
    s->drawRoundRect(x, y, w, h, 12, COLOR_TILE_BORDER);
}

enum LabelSize { LBL_TINY, LBL_SMALL, LBL_MEDIUM, LBL_LARGE, LBL_XLARGE };

static void draw_label_center(LGFX_Sprite* s, int32_t cx, int32_t cy, LabelSize sz,
                               uint32_t color, const char* text) {
    switch (sz) {
        case LBL_TINY:   font_tiny  (s); break;
        case LBL_SMALL:  font_small (s); break;
        case LBL_MEDIUM: font_medium(s); break;
        case LBL_LARGE:  font_large (s); break;
        case LBL_XLARGE: font_xlarge(s); break;
    }
    s->setTextColor(color);
    s->setTextDatum(textdatum_t::middle_center);
    s->drawString(text, cx, cy);
}

// Draw an arc gauge: start angle 135°, span 270°, value in [min,max]
static void draw_arc(LGFX_Sprite* s, int32_t cx, int32_t cy, int32_t radius,
                     int32_t min_val, int32_t max_val, int32_t value,
                     uint32_t arc_color) {
    const int32_t START_DEG = 135;
    const int32_t SPAN_DEG  = 270;
    const int32_t THICK     = 12;

    // background arc
    for (int32_t t = 0; t < THICK; t++) {
        s->drawArc(cx, cy, radius - t, radius - t,
                   START_DEG, START_DEG + SPAN_DEG, COLOR_TILE_BORDER);
    }

    // value arc
    int32_t val_deg = (int32_t)((float)(value - min_val) / (max_val - min_val) * SPAN_DEG);
    if (val_deg > 0) {
        for (int32_t t = 0; t < THICK; t++) {
            s->drawArc(cx, cy, radius - t, radius - t,
                       START_DEG, START_DEG + val_deg, arc_color);
        }
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

bool dashboard_draw(LGFX_Sprite* spr, bool force) {
    if (!spr) return false;

    VehicleData d;
    if (xSemaphoreTake(gVehicleDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        d = gVehicleData;
        xSemaphoreGive(gVehicleDataMutex);
    } else {
        return false;
    }

    // Skip redraw if data is identical to last rendered frame and not forced
    if (sHasRendered && !force && memcmp(&d, &sLastData, sizeof(VehicleData)) == 0) {
        return false;
    }
    sLastData    = d;
    sHasRendered = true;

    int32_t W = spr->width();
    int32_t H = spr->height();

    spr->fillScreen(COLOR_BG);

    char buf[32];
    int32_t CENTER_W = W - 2 * (TILE_SM + PAD * 2);

    // ── Row 1 ─────────────────────────────────────────────────────────────────
    int32_t row1_y = PAD;

    // Coolant tile
    draw_tile(spr, PAD, row1_y, TILE_SM, ROW1_H);
    draw_label_center(spr, PAD + TILE_SM/2, row1_y + 20, LBL_TINY,
                      COLOR_TEXT_SECONDARY, "COOLANT");
    uint32_t c_col = theme_threshold_color(d.coolantTempC, COOLANT_WARN_C, COOLANT_CRIT_C);
    snprintf(buf, sizeof(buf), "%.0f F", d.coolantTempC * 9.0f / 5.0f + 32.0f);
    draw_label_center(spr, PAD + TILE_SM/2, row1_y + ROW1_H/2,
                      LBL_MEDIUM, c_col, buf);

    // RPM tile (center)
    int32_t rpm_x = PAD + TILE_SM + PAD;
    draw_tile(spr, rpm_x, row1_y, CENTER_W, ROW1_H);
    draw_label_center(spr, rpm_x + CENTER_W/2, row1_y + 20,
                      LBL_TINY, COLOR_TEXT_SECONDARY, "RPM");
    draw_arc(spr, rpm_x + CENTER_W/2, row1_y + ROW1_H/2, 120,
             0, 8000, (int32_t)d.rpm, COLOR_ACCENT);
    snprintf(buf, sizeof(buf), "%.0f", d.rpm);
    draw_label_center(spr, rpm_x + CENTER_W/2, row1_y + ROW1_H/2,
                      LBL_XLARGE, COLOR_TEXT_PRIMARY, buf);

    // Oil tile
    int32_t oil_x = rpm_x + CENTER_W + PAD;
    draw_tile(spr, oil_x, row1_y, TILE_SM, ROW1_H);
    draw_label_center(spr, oil_x + TILE_SM/2, row1_y + 20,
                      LBL_TINY, COLOR_TEXT_SECONDARY, "OIL");
    uint32_t o_col = theme_threshold_color(d.oilTempC, OIL_WARN_C, OIL_CRIT_C);
    snprintf(buf, sizeof(buf), "%.0f F", d.oilTempC * 9.0f / 5.0f + 32.0f);
    draw_label_center(spr, oil_x + TILE_SM/2, row1_y + ROW1_H/2,
                      LBL_MEDIUM, o_col, buf);

    // ── Row 2 ─────────────────────────────────────────────────────────────────
    int32_t row2_y = row1_y + ROW1_H + PAD;
    int32_t tile4_w = (W - 5 * PAD) / 4;

    // Speed
    draw_tile(spr, PAD, row2_y, tile4_w, ROW2_H);
    draw_label_center(spr, PAD + tile4_w/2, row2_y + 16,
                      LBL_TINY, COLOR_TEXT_SECONDARY,
                      SPEED_UNIT_MPH ? "mph" : "km/h");
    float spd = SPEED_UNIT_MPH ? d.speedKmh * 0.621371f : d.speedKmh;
    snprintf(buf, sizeof(buf), "%.0f", spd);
    draw_label_center(spr, PAD + tile4_w/2, row2_y + ROW2_H/2,
                      LBL_XLARGE, COLOR_TEXT_PRIMARY, buf);

    // Engine load arc
    int32_t load_x = PAD * 2 + tile4_w;
    draw_tile(spr, load_x, row2_y, tile4_w, ROW2_H);
    draw_label_center(spr, load_x + tile4_w/2, row2_y + 16,
                      LBL_TINY, COLOR_TEXT_SECONDARY, "LOAD %");
    uint32_t l_col = theme_threshold_color(d.engineLoadPct,
                                           ENGINE_LOAD_WARN_PCT, ENGINE_LOAD_CRIT_PCT);
    draw_arc(spr, load_x + tile4_w/2, row2_y + ROW2_H/2 + 10, 55,
             0, 100, (int32_t)d.engineLoadPct, l_col);

    // Throttle arc
    int32_t thr_x = PAD * 3 + tile4_w * 2;
    draw_tile(spr, thr_x, row2_y, tile4_w, ROW2_H);
    draw_label_center(spr, thr_x + tile4_w/2, row2_y + 16,
                      LBL_TINY, COLOR_TEXT_SECONDARY, "THROTTLE %");
    draw_arc(spr, thr_x + tile4_w/2, row2_y + ROW2_H/2 + 10, 55,
             0, 100, (int32_t)d.throttlePosPct, COLOR_ACCENT2);

    // Voltage
    int32_t volt_x = PAD * 4 + tile4_w * 3;
    draw_tile(spr, volt_x, row2_y, tile4_w, ROW2_H);
    draw_label_center(spr, volt_x + tile4_w/2, row2_y + 16,
                      LBL_TINY, COLOR_TEXT_SECONDARY, "BATT V");
    snprintf(buf, sizeof(buf), "%.2fV", d.ctrlModuleVoltage);
    draw_label_center(spr, volt_x + tile4_w/2, row2_y + ROW2_H/2,
                      LBL_MEDIUM, COLOR_TEXT_PRIMARY, buf);

    // ── Status bar ────────────────────────────────────────────────────────────
    int32_t status_y = row2_y + ROW2_H + PAD;
    draw_tile(spr, PAD, status_y, W - 2 * PAD, STATUS_H);

    uint32_t conn_col = d.btConnected ? (uint32_t)COLOR_OK : (uint32_t)COLOR_TEXT_SECONDARY;
    draw_label_center(spr, PAD + 100, status_y + STATUS_H/2,
                      LBL_TINY, conn_col,
                      d.btConnected ? "OBD Connected" : "OBD Disconnected");

    snprintf(buf, sizeof(buf), "Protocol: %s", d.obdProtocol[0] ? d.obdProtocol : "--");
    draw_label_center(spr, W/2, status_y + STATUS_H/2,
                      LBL_TINY, COLOR_TEXT_SECONDARY, buf);

    snprintf(buf, sizeof(buf), "Intake: %.0f F", d.intakeAirTempC * 9.0f / 5.0f + 32.0f);
    draw_label_center(spr, W - PAD - 120, status_y + STATUS_H/2,
                      LBL_TINY, COLOR_TEXT_SECONDARY, buf);

    return true;
}
