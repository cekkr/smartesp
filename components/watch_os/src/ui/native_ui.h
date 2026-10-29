#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    char hour_minute[8];
    char seconds[4];
    char date_line[48];
} watch_ui_clock_model_t;

typedef enum {
    WATCH_UI_ACTION_BUTTON,
    WATCH_UI_ACTION_WATCH,
    WATCH_UI_ACTION_SETTINGS,
    WATCH_UI_ACTION_DIM,
    WATCH_UI_ACTION_BRIGHTEN,
} watch_ui_action_t;

typedef void (*watch_ui_action_handler_t)(watch_ui_action_t action);

void watch_native_ui_set_action_handler(watch_ui_action_handler_t handler);
void watch_native_ui_request(watch_ui_action_t action);
esp_err_t watch_native_ui_show_apps(void);
esp_err_t watch_native_ui_show_settings(uint8_t brightness, bool sd_available);
esp_err_t watch_native_ui_show_app_error(const char *title);
esp_err_t watch_native_ui_inject_tap(int x, int y);
esp_err_t watch_native_ui_snapshot(void);

esp_err_t watch_native_ui_show_boot(const char *stage, uint8_t progress);
esp_err_t watch_native_ui_show_clock(const watch_ui_clock_model_t *model);
