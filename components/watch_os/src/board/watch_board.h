#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

esp_err_t watch_board_init(void);
bool watch_board_lock(uint32_t timeout_ms);
void watch_board_unlock(void);
lv_display_t *watch_board_display(void);
esp_err_t watch_board_present_first_frame(void);
