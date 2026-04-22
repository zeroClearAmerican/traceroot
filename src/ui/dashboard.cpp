#include "dashboard.h"
#include "theme.h"
#include "../pid_poller.h"
#include "../config.h"
#include <lvgl.h>

// ─── Widget handles ───────────────────────────────────────────────────────────
static lv_obj_t* scr              = nullptr;

// Center: RPM arc
static lv_obj_t* arc_rpm         = nullptr;
static lv_obj_t* lbl_rpm         = nullptr;

// Top-left / top-right tiles
static lv_obj_t* lbl_coolant     = nullptr;
static lv_obj_t* lbl_oil         = nullptr;

// Bottom row
static lv_obj_t* lbl_speed       = nullptr;
static lv_obj_t* arc_load        = nullptr;
static lv_obj_t* arc_throttle    = nullptr;
static lv_obj_t* lbl_voltage     = nullptr;

// Status bar
static lv_obj_t* lbl_bt_status   = nullptr;
static lv_obj_t* lbl_protocol    = nullptr;
static lv_obj_t* lbl_intake      = nullptr;

// ─── Internal helpers ─────────────────────────────────────────────────────────

static lv_obj_t* make_tile(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, w, h);
    lv_obj_set_style_bg_color(tile, theme_color(COLOR_TILE_BG), 0);
    lv_obj_set_style_border_color(tile, theme_color(COLOR_TILE_BORDER), 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_radius(tile, 12, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    return tile;
}

static lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_font_t* font) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, theme_color(COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(lbl);
    return lbl;
}

static lv_obj_t* make_arc_gauge(lv_obj_t* parent, int32_t size,
                                  int32_t min_val, int32_t max_val) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, min_val, max_val);
    lv_arc_set_value(arc, min_val);
    lv_obj_set_style_arc_color(arc, theme_color(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, theme_color(COLOR_TILE_BORDER), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);  // hide knob
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(arc);
    return arc;
}

// ─── Public API ───────────────────────────────────────────────────────────────

void dashboard_init() {
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color(COLOR_BG), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t W = DISPLAY_WIDTH;
    const int32_t H = DISPLAY_HEIGHT;
    const int32_t PAD = 12;
    const int32_t TILE_SM = 180;
    const int32_t ROW1_H  = 320;
    const int32_t ROW2_H  = 160;
    const int32_t STATUS_H = 48;
    const int32_t CENTER_W = W - 2 * (TILE_SM + PAD * 2);

    // ── Row 1: coolant | RPM arc | oil ───────────────────────────────────────
    // Coolant tile
    lv_obj_t* tile_coolant = make_tile(scr, PAD, PAD, TILE_SM, ROW1_H);
    lv_obj_t* lbl_ct = make_label(tile_coolant, "COOLANT", FONT_SMALL);
    lv_obj_align(lbl_ct, LV_ALIGN_TOP_MID, 0, 8);
    lbl_coolant = make_label(tile_coolant, "--°C", FONT_MEDIUM);
    lv_obj_align(lbl_coolant, LV_ALIGN_CENTER, 0, 10);

    // RPM tile (center)
    int32_t rpm_x = PAD + TILE_SM + PAD;
    lv_obj_t* tile_rpm = make_tile(scr, rpm_x, PAD, CENTER_W, ROW1_H);
    lv_obj_t* lbl_rh = make_label(tile_rpm, "RPM", FONT_SMALL);
    lv_obj_align(lbl_rh, LV_ALIGN_TOP_MID, 0, 8);
    arc_rpm = make_arc_gauge(tile_rpm, 260, 0, 8000);
    lv_obj_align(arc_rpm, LV_ALIGN_CENTER, 0, 10);
    lbl_rpm = make_label(tile_rpm, "0", FONT_LARGE);
    lv_obj_align(lbl_rpm, LV_ALIGN_CENTER, 0, 10);

    // Oil tile
    int32_t oil_x = rpm_x + CENTER_W + PAD;
    lv_obj_t* tile_oil = make_tile(scr, oil_x, PAD, TILE_SM, ROW1_H);
    lv_obj_t* lbl_oh = make_label(tile_oil, "OIL", FONT_SMALL);
    lv_obj_align(lbl_oh, LV_ALIGN_TOP_MID, 0, 8);
    lbl_oil = make_label(tile_oil, "--°C", FONT_MEDIUM);
    lv_obj_align(lbl_oil, LV_ALIGN_CENTER, 0, 10);

    // ── Row 2: speed | load arc | throttle arc | voltage ─────────────────────
    int32_t row2_y = PAD + ROW1_H + PAD;
    int32_t tile4_w = (W - 5 * PAD) / 4;

    // Speed
    lv_obj_t* tile_spd = make_tile(scr, PAD, row2_y, tile4_w, ROW2_H);
    lv_obj_t* lbl_sh = make_label(tile_spd, SPEED_UNIT_MPH ? "mph" : "km/h", FONT_SMALL);
    lv_obj_align(lbl_sh, LV_ALIGN_TOP_MID, 0, 6);
    lbl_speed = make_label(tile_spd, "0", FONT_LARGE);
    lv_obj_align(lbl_speed, LV_ALIGN_CENTER, 0, 10);

    // Engine load arc
    lv_obj_t* tile_load = make_tile(scr, PAD * 2 + tile4_w, row2_y, tile4_w, ROW2_H);
    lv_obj_t* lbl_lh = make_label(tile_load, "LOAD %", FONT_SMALL);
    lv_obj_align(lbl_lh, LV_ALIGN_TOP_MID, 0, 6);
    arc_load = make_arc_gauge(tile_load, 100, 0, 100);
    lv_obj_align(arc_load, LV_ALIGN_CENTER, 0, 10);

    // Throttle arc
    lv_obj_t* tile_thr = make_tile(scr, PAD * 3 + tile4_w * 2, row2_y, tile4_w, ROW2_H);
    lv_obj_t* lbl_th = make_label(tile_thr, "THROTTLE %", FONT_SMALL);
    lv_obj_align(lbl_th, LV_ALIGN_TOP_MID, 0, 6);
    arc_throttle = make_arc_gauge(tile_thr, 100, 0, 100);
    lv_obj_align(arc_throttle, LV_ALIGN_CENTER, 0, 10);

    // Voltage
    lv_obj_t* tile_volt = make_tile(scr, PAD * 4 + tile4_w * 3, row2_y, tile4_w, ROW2_H);
    lv_obj_t* lbl_vh = make_label(tile_volt, "BATT V", FONT_SMALL);
    lv_obj_align(lbl_vh, LV_ALIGN_TOP_MID, 0, 6);
    lbl_voltage = make_label(tile_volt, "--V", FONT_MEDIUM);
    lv_obj_align(lbl_voltage, LV_ALIGN_CENTER, 0, 10);

    // ── Status bar ────────────────────────────────────────────────────────────
    int32_t status_y = row2_y + ROW2_H + PAD;
    lv_obj_t* status_bar = make_tile(scr, PAD, status_y, W - 2 * PAD, STATUS_H);
    lv_obj_set_style_radius(status_bar, 8, 0);

    lbl_bt_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_bt_status, LV_SYMBOL_BLUETOOTH " Disconnected");
    lv_obj_set_style_text_color(lbl_bt_status, theme_color(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lbl_bt_status, FONT_SMALL, 0);
    lv_obj_align(lbl_bt_status, LV_ALIGN_LEFT_MID, 10, 0);

    lbl_protocol = lv_label_create(status_bar);
    lv_label_set_text(lbl_protocol, "Protocol: --");
    lv_obj_set_style_text_color(lbl_protocol, theme_color(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lbl_protocol, FONT_SMALL, 0);
    lv_obj_align(lbl_protocol, LV_ALIGN_CENTER, 0, 0);

    lbl_intake = lv_label_create(status_bar);
    lv_label_set_text(lbl_intake, "Intake: --°C");
    lv_obj_set_style_text_color(lbl_intake, theme_color(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lbl_intake, FONT_SMALL, 0);
    lv_obj_align(lbl_intake, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_scr_load(scr);
}

void dashboard_update() {
    // Safely copy data
    VehicleData d;
    if (xSemaphoreTake(gVehicleDataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        d = gVehicleData;
        xSemaphoreGive(gVehicleDataMutex);
    } else {
        return;
    }

    // RPM
    lv_arc_set_value(arc_rpm, (int32_t)d.rpm);
    lv_label_set_text_fmt(lbl_rpm, "%.0f", d.rpm);

    // Speed
#if SPEED_UNIT_MPH
    lv_label_set_text_fmt(lbl_speed, "%.0f", d.speedKmh * 0.621371f);
#else
    lv_label_set_text_fmt(lbl_speed, "%.0f", d.speedKmh);
#endif

    // Coolant
    lv_color_t c_col = theme_threshold_color(d.coolantTempC, COOLANT_WARN_C, COOLANT_CRIT_C);
    lv_label_set_text_fmt(lbl_coolant, "%.0f°C", d.coolantTempC);
    lv_obj_set_style_text_color(lbl_coolant, c_col, 0);

    // Oil
    lv_color_t o_col = theme_threshold_color(d.oilTempC, OIL_WARN_C, OIL_CRIT_C);
    lv_label_set_text_fmt(lbl_oil, "%.0f°C", d.oilTempC);
    lv_obj_set_style_text_color(lbl_oil, o_col, 0);

    // Load arc
    lv_arc_set_value(arc_load, (int32_t)d.engineLoadPct);
    lv_color_t l_col = theme_threshold_color(d.engineLoadPct, ENGINE_LOAD_WARN_PCT, ENGINE_LOAD_CRIT_PCT);
    lv_obj_set_style_arc_color(arc_load, l_col, LV_PART_INDICATOR);

    // Throttle arc
    lv_arc_set_value(arc_throttle, (int32_t)d.throttlePosPct);

    // Voltage
    lv_label_set_text_fmt(lbl_voltage, "%.2fV", d.ctrlModuleVoltage);

    // Intake
    lv_label_set_text_fmt(lbl_intake, "Intake: %.0f°C", d.intakeAirTempC);

    // BT status
    if (d.btConnected) {
        lv_label_set_text(lbl_bt_status, LV_SYMBOL_BLUETOOTH " Connected");
        lv_obj_set_style_text_color(lbl_bt_status, theme_color(COLOR_OK), 0);
    } else {
        lv_label_set_text(lbl_bt_status, LV_SYMBOL_BLUETOOTH " Disconnected");
        lv_obj_set_style_text_color(lbl_bt_status, theme_color(COLOR_TEXT_SECONDARY), 0);
    }

    // Protocol
    lv_label_set_text_fmt(lbl_protocol, "Protocol: %s", d.obdProtocol[0] ? d.obdProtocol : "--");
}
