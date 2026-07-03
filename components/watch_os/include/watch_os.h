#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool prefer_sdcard_runtime;
    bool auto_launch_demo_app;
    uint32_t internal_ram_floor_bytes;
    uint32_t psram_floor_bytes;
} watch_os_config_t;

#define WATCH_OS_DEFAULT_CONFIG()                       \
    {                                                   \
        .prefer_sdcard_runtime = true,                  \
        .auto_launch_demo_app = true,                   \
        .internal_ram_floor_bytes = 96 * 1024,          \
        .psram_floor_bytes = 512 * 1024,                \
    }

esp_err_t watch_os_start(const watch_os_config_t *config);
