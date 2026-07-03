#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char headline[96];
    char memory_line[96];
    char apps_line[512];
    char footer[96];
} watch_ui_shell_model_t;

esp_err_t watch_native_ui_show_boot(const char *stage, uint8_t progress);
esp_err_t watch_native_ui_show_shell(const watch_ui_shell_model_t *model);
