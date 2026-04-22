#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>

#include "config.h"
#include "elm327.h"
#include "pid_poller.h"
#include "ui/dashboard.h"

// ─── LVGL display flush callback (via M5GFX) ─────────────────────────────────
static M5GFX* gGfx = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gGfx->startWrite();
    gGfx->setAddrWindow(area->x1, area->y1, w, h);
    gGfx->writePixels((lgfx::rgb565_t*)px_map, w * h);
    gGfx->endWrite();
    lv_display_flush_ready(disp);
}

// ─── LVGL touch input callback ────────────────────────────────────────────────
static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto& touch = M5.Touch;
    M5.update();
    if (touch.getCount() > 0) {
        auto t = touch.getDetail(0);
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = t.x;
        data->point.y = t.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ─── LVGL draw buffer ─────────────────────────────────────────────────────────
static lv_color_t draw_buf[DISPLAY_WIDTH * LVGL_DRAW_BUF_LINES];
static lv_display_t* lvgl_disp = nullptr;

// ─── BT connection task (core 0) ──────────────────────────────────────────────
static ELM327Client elm;

static void bt_connect_task(void* param) {
    Serial.println("[BT] Connecting to OBD adapter...");
    while (!elm.connect(OBD_BT_MAC)) {
        Serial.println("[BT] Retrying in 3s...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    if (xSemaphoreTake(gVehicleDataMutex, portMAX_DELAY)) {
        gVehicleData.btConnected = true;
        xSemaphoreGive(gVehicleDataMutex);
    }
    // Hand off to PID poller (already created, will detect isConnected())
    vTaskDelete(nullptr);
}

// ─── LVGL periodic tick (called from timer ISR) ───────────────────────────────
static void IRAM_ATTR lvgl_tick_cb(void* arg) {
    lv_tick_inc(5);
}

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // M5Unified — handles display, touch, power
    auto cfg = M5.config();
    M5.begin(cfg);
    gGfx = &M5.Display;
    gGfx->setRotation(1);  // landscape

    Serial.printf("[MAIN] Display: %dx%d\n", gGfx->width(), gGfx->height());

    // ── LVGL init ────────────────────────────────────────────────────────────
    lv_init();

    lvgl_disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(lvgl_disp, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_disp, draw_buf, nullptr,
                           sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, lvgl_touch_cb);

    // 5 ms LVGL tick using ESP32 hardware timer
    esp_timer_handle_t lvgl_timer;
    esp_timer_create_args_t timer_args = {
        .callback        = lvgl_tick_cb,
        .arg             = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "lvgl_tick",
    };
    esp_timer_create(&timer_args, &lvgl_timer);
    esp_timer_start_periodic(lvgl_timer, 5000);  // 5000 µs = 5 ms

    // ── Build dashboard UI ────────────────────────────────────────────────────
    dashboard_init();

    // ── Start PID poller task (core 0) ────────────────────────────────────────
    pidPollerStart(elm);

    // ── Start BT connection task (core 0) ─────────────────────────────────────
    xTaskCreatePinnedToCore(bt_connect_task, "bt_connect", 4096, nullptr, 1, nullptr, 0);

    Serial.println("[MAIN] Setup complete.");
}

// ─── loop() — LVGL handler on core 1 ─────────────────────────────────────────
void loop() {
    lv_task_handler();
    dashboard_update();
    delay(5);
}
