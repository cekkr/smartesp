#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char headline[128];
    char memory_line[128];
    char apps_line[768];
    char footer[160];
} watch_ui_shell_model_t;

esp_err_t watch_native_ui_show_boot(const char *stage, uint8_t progress);
esp_err_t watch_native_ui_show_shell(const watch_ui_shell_model_t *model);
