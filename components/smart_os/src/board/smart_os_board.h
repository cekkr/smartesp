#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

esp_err_t smart_os_board_init(void);
bool smart_os_board_lock(uint32_t timeout_ms);
void smart_os_board_unlock(void);
lv_display_t *smart_os_board_display(void);
esp_err_t smart_os_board_present_first_frame(void);
bool smart_os_board_poll_button(void);
esp_err_t smart_os_board_set_brightness(uint8_t percent);
