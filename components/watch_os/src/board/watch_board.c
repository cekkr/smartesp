#include "watch_board.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"

#define WATCH_DRAW_BUFFER_LINES 40

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
static lv_indev_t *s_touch_input;
static bool s_first_frame_presented;

static void round_sh8601_area(lv_area_t *area)
{
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

esp_err_t watch_board_init(void)
{
    if (s_display != NULL) {
        return ESP_OK;
    }

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_touch_handle_t touch = NULL;
    const bsp_display_config_t panel_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8,
    };
    const lvgl_port_cfg_t port_cfg = WATCH_LVGL_PORT_INIT_CONFIG();

    ESP_RETURN_ON_ERROR(bsp_display_new(&panel_cfg, &panel, &io), TAG, "panel start failed");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_off(), TAG, "panel blanking failed");
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "LVGL port start failed");

    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BSP_LCD_H_RES * WATCH_DRAW_BUFFER_LINES,
        .double_buffer = false,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .rounder_cb = round_sh8601_area,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
            .sw_rotate = false,
            .swap_bytes = true,
        },
    };

    // This panel is QSPI, not RGB. The generic display registration waits for
    // each asynchronous color transfer before LVGL reuses its draw buffer.
    s_display = lvgl_port_add_disp(&display_cfg);
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_FAIL, TAG, "display registration failed");

    ESP_RETURN_ON_ERROR(bsp_touch_new(NULL, &touch), TAG, "touch start failed");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = s_display,
        .handle = touch,
    };
    s_touch_input = lvgl_port_add_touch(&touch_cfg);
    ESP_RETURN_ON_FALSE(s_touch_input != NULL, ESP_FAIL, TAG, "touch registration failed");

    return ESP_OK;
}

bool watch_board_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void watch_board_unlock(void)
{
    lvgl_port_unlock();
}

lv_display_t *watch_board_display(void)
{
    return s_display;
}

esp_err_t watch_board_present_first_frame(void)
{
    if ((s_display == NULL) || s_first_frame_presented) {
        return ESP_OK;
    }

    lv_refr_now(s_display);
    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "display enable failed");
    s_first_frame_presented = true;
    return ESP_OK;
}
