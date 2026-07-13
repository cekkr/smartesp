#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char hour_minute[8];
    char seconds[4];
    char date_line[48];
} watch_ui_clock_model_t;

esp_err_t watch_native_ui_show_boot(const char *stage, uint8_t progress);
esp_err_t watch_native_ui_show_clock(const watch_ui_clock_model_t *model);
