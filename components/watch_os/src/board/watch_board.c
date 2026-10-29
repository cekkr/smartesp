#include "watch_board.h"

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"

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
static i2c_master_dev_handle_t s_power_button;
static i2c_master_dev_handle_t s_pmic;
static bool s_button_raw;
static bool s_button_stable;
static int64_t s_button_changed;

static void init_buttons(void)
{
    const gpio_config_t boot = {
        .pin_bit_mask = 1ULL << GPIO_NUM_0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&boot));
    // PWR is active-high on TCA9554 EXIO6; keep the PMIC's long-press behavior.
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (i2c_master_probe(bus, 0x20, 100) == ESP_OK) {
        const i2c_device_config_t device = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x20,
            .scl_speed_hz = 100000,
        };
        if (i2c_master_bus_add_device(bus, &device, &s_power_button) == ESP_OK) {
            uint8_t reg = 3, direction = 0;
            if (i2c_master_transmit_receive(s_power_button, &reg, 1, &direction, 1, 100) == ESP_OK) {
                const uint8_t config[] = {3, direction | (1 << 6)};
                (void)i2c_master_transmit(s_power_button, config, sizeof(config), 100);
            }
        }
    }
    if (s_power_button == NULL && i2c_master_probe(bus, 0x34, 100) == ESP_OK) {
        const i2c_device_config_t device = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x34,
            .scl_speed_hz = 100000,
        };
        if (i2c_master_bus_add_device(bus, &device, &s_pmic) == ESP_OK) {
            // Older boards expose PWR through AXP2101 INTSTS2, not EXIO6.
            // Enable only short-press bit 3, preserving power/charger settings.
            uint8_t reg = 0x41, enabled = 0;
            esp_err_t err = i2c_master_transmit_receive(s_pmic, &reg, 1, &enabled, 1, 100);
            const uint8_t enable[] = {0x41, enabled | 0x08};
            const uint8_t clear[] = {0x49, 0x08};
            if (err == ESP_OK) err = i2c_master_transmit(s_pmic, enable, sizeof(enable), 100);
            if (err == ESP_OK) err = i2c_master_transmit(s_pmic, clear, sizeof(clear), 100);
            if (err != ESP_OK) {
                (void)i2c_master_bus_rm_device(s_pmic);
                s_pmic = NULL;
            }
        }
    }
    ESP_LOGI(TAG, "buttons ready BOOT=GPIO0 PWR=%s debounce=40ms",
             s_power_button ? "EXIO6" : (s_pmic ? "AXP2101" : "unavailable"));
}

bool watch_board_poll_button(void)
{
    bool pmic_click = false;
    if (s_pmic != NULL) {
        uint8_t reg = 0x49, events = 0;
        if (i2c_master_transmit_receive(s_pmic, &reg, 1, &events, 1, 20) == ESP_OK && (events & 0x08)) {
            const uint8_t clear[] = {0x49, 0x08};
            // Acknowledge before dispatch so a held/latched event cannot repeat.
            if (i2c_master_transmit(s_pmic, clear, sizeof(clear), 20) == ESP_OK) {
                ESP_LOGI(TAG, "side-button pressed source=AXP2101");
                pmic_click = true;
            }
        }
    }
    bool pressed = gpio_get_level(GPIO_NUM_0) == 0;
    if (s_power_button != NULL) {
        uint8_t reg = 0, inputs = 0;
        if (i2c_master_transmit_receive(s_power_button, &reg, 1, &inputs, 1, 20) == ESP_OK) {
            pressed |= (inputs & (1 << 6)) != 0;
        } else {
            // Do not synthesize release/press pairs after a failed I2C read.
            return pmic_click;
        }
    }
    const int64_t now = esp_timer_get_time();
    if (pressed != s_button_raw) {
        s_button_raw = pressed;
        s_button_changed = now;
    }
    if ((now - s_button_changed >= 40000) && s_button_stable != s_button_raw) {
        s_button_stable = s_button_raw;
        if (s_button_stable) {
            ESP_LOGI(TAG, "side-button pressed");
            return true;
        }
    }
    return pmic_click;
}

esp_err_t watch_board_set_brightness(uint8_t percent)
{
    if (percent < 20 || percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!watch_board_lock(200)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = bsp_display_brightness_set(percent);
    watch_board_unlock();
    return err;
}

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

    init_buttons();

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
