#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    char hour_minute[8];
    char seconds[4];
    char date_line[48];
} smart_os_ui_clock_model_t;

typedef enum {
    SMART_OS_UI_ACTION_BUTTON,
    SMART_OS_UI_ACTION_WATCH,
    SMART_OS_UI_ACTION_SETTINGS,
    SMART_OS_UI_ACTION_DIM,
    SMART_OS_UI_ACTION_BRIGHTEN,
} smart_os_ui_action_t;

typedef void (*smart_os_ui_action_handler_t)(smart_os_ui_action_t action);

void smart_os_native_ui_set_action_handler(smart_os_ui_action_handler_t handler);
void smart_os_native_ui_request(smart_os_ui_action_t action);
esp_err_t smart_os_native_ui_show_apps(void);
esp_err_t smart_os_native_ui_show_settings(uint8_t brightness, bool sd_available);
esp_err_t smart_os_native_ui_show_app_error(const char *title);
esp_err_t smart_os_native_ui_inject_tap(int x, int y);
esp_err_t smart_os_native_ui_snapshot(void);

esp_err_t smart_os_native_ui_show_boot(const char *stage, uint8_t progress);
esp_err_t smart_os_native_ui_show_clock(const smart_os_ui_clock_model_t *model);
