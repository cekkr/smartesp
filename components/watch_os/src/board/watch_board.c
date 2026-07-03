#include "watch_board.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"

#define WATCH_LVGL_PORT_INIT_CONFIG()   \
    {                                   \
        .task_priority = 4,             \
        .task_stack = 10 * 1024,        \
        .task_affinity = -1,            \
        .task_max_sleep_ms = 500,       \
        .timer_period_ms = 5,           \
    }

static const char *TAG = "watch_board";
static lv_display_t *s_display;

esp_err_t watch_board_init(void)
{
    if (s_display != NULL) {
        return ESP_OK;
    }

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = WATCH_LVGL_PORT_INIT_CONFIG(),
    };

    s_display = bsp_display_start_with_config(&cfg);
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_FAIL, TAG, "display start failed");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "backlight enable failed");

    return ESP_OK;
}

bool watch_board_lock(uint32_t timeout_ms)
{
    return bsp_display_lock(timeout_ms);
}

void watch_board_unlock(void)
{
    bsp_display_unlock();
}

lv_display_t *watch_board_display(void)
{
    return s_display;
}
