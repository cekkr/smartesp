#include "native_ui.h"

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "smart_os_board.h"

static smart_os_ui_action_handler_t s_handler;
static lv_obj_t *s_apps;
static lv_obj_t *s_settings;
static lv_obj_t *s_brightness;
static lv_obj_t *s_brightness_bar;
static lv_obj_t *s_storage;
static lv_obj_t *s_error;
static lv_obj_t *s_error_title;

void smart_os_native_ui_set_action_handler(smart_os_ui_action_handler_t handler)
{
    s_handler = handler;
}

void smart_os_native_ui_request(smart_os_ui_action_t action)
{
    if (s_handler != NULL) {
        s_handler(action);
    }
}

static void clicked(lv_event_t *event)
{
    smart_os_native_ui_request((smart_os_ui_action_t)(uintptr_t)lv_event_get_user_data(event));
}

static lv_obj_t *surface(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *screen(void)
{
    lv_obj_t *obj = lv_obj_create(NULL);
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, width);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static void make_button(lv_obj_t *obj, smart_os_ui_action_t action)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_transform_scale(obj, 238, LV_STATE_PRESSED);
    lv_obj_add_event_cb(obj, clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
}

static void line(lv_obj_t *parent, const lv_point_precise_t *points, int count, uint32_t color, int width)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_line_set_points(obj, points, count);
    lv_obj_set_style_line_width(obj, width, 0);
    lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
}

static void ensure_apps(void)
{
    if (s_apps != NULL) {
        return;
    }
    s_apps = screen();
    label(s_apps, "Apps", 40, 64, 330, &lv_font_montserrat_32, 0xf6f7fa);
    label(s_apps, "Choose your next moment", 40, 108, 330, &lv_font_montserrat_14, 0x7d8998);

    lv_obj_t *watch = surface(s_apps, 48, 157, 150, 150, 0x142f2b, LV_RADIUS_CIRCLE);
    make_button(watch, SMART_OS_UI_ACTION_WATCH);
    lv_obj_add_flag(watch, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_style_border_width(watch, 2, 0);
    lv_obj_set_style_border_color(watch, lv_color_hex(0x2c6256), 0);
    lv_obj_t *face = surface(watch, 23, 23, 104, 104, 0xf5f3eb, LV_RADIUS_CIRCLE);
    // Bubble child touch targets into the circle without intercepting its click.
    surface(face, 50, 8, 4, 10, 0x26332f, 2);
    surface(face, 50, 86, 4, 10, 0x26332f, 2);
    surface(face, 8, 50, 10, 4, 0x26332f, 2);
    surface(face, 86, 50, 10, 4, 0x26332f, 2);
    static const lv_point_precise_t hour[] = {{52, 52}, {32, 34}};
    static const lv_point_precise_t minute[] = {{52, 52}, {75, 24}};
    static const lv_point_precise_t second[] = {{52, 61}, {52, 20}};
    line(face, hour, 2, 0x18221f, 7);
    line(face, minute, 2, 0x18221f, 5);
    line(face, second, 2, 0xf16c45, 2);
    surface(face, 48, 48, 9, 9, 0xf16c45, LV_RADIUS_CIRCLE);
    label(s_apps, "Watch", 38, 321, 170, &lv_font_montserrat_18, 0xe9f5ef);

    lv_obj_t *settings = surface(s_apps, 224, 231, 136, 136, 0x252b37, LV_RADIUS_CIRCLE);
    make_button(settings, SMART_OS_UI_ACTION_SETTINGS);
    lv_obj_add_flag(settings, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_style_border_width(settings, 2, 0);
    lv_obj_set_style_border_color(settings, lv_color_hex(0x434d60), 0);
    lv_obj_t *gear = label(settings, LV_SYMBOL_SETTINGS, 0, 0, 136, &lv_font_montserrat_48, 0xdce3ef);
    lv_obj_center(gear);
    label(s_apps, "Settings", 207, 380, 170, &lv_font_montserrat_18, 0xdce3ef);
    label(s_apps, "Side button returns to Watch", 45, 428, 320, &lv_font_montserrat_12, 0x697787);
}

esp_err_t smart_os_native_ui_show_apps(void)
{
    if (!smart_os_board_lock(200)) {
        return ESP_ERR_TIMEOUT;
    }
    ensure_apps();
    lv_screen_load(s_apps);
    smart_os_board_unlock();
    ESP_LOGI("smart_os_ui", "apps-ui rendered apps=Watch,Settings");
    return ESP_OK;
}

static void ensure_settings(void)
{
    if (s_settings != NULL) {
        return;
    }
    s_settings = screen();
    label(s_settings, "Settings", 45, 64, 320, &lv_font_montserrat_32, 0xf6f7fa);
    label(s_settings, "Make it yours", 45, 110, 320, &lv_font_montserrat_14, 0x7d8998);
    lv_obj_t *card = surface(s_settings, 35, 157, 340, 220, 0x141a22, 32);
    label(card, "DISPLAY BRIGHTNESS", 0, 22, 340, &lv_font_montserrat_12, 0x99a9bd);
    s_brightness = label(card, "80%", 0, 52, 340, &lv_font_montserrat_48, 0xf6f7fa);
    s_brightness_bar = lv_bar_create(card);
    lv_obj_set_size(s_brightness_bar, 164, 8);
    lv_obj_set_pos(s_brightness_bar, 88, 120);
    lv_obj_set_style_bg_color(s_brightness_bar, lv_color_hex(0x2c3745), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_brightness_bar, lv_color_hex(0x75e6c5), LV_PART_INDICATOR);
    lv_bar_set_range(s_brightness_bar, 20, 100);
    lv_obj_t *minus = surface(card, 32, 149, 112, 52, 0x293443, LV_RADIUS_CIRCLE);
    make_button(minus, SMART_OS_UI_ACTION_DIM);
    label(minus, LV_SYMBOL_MINUS, 0, 12, 112, &lv_font_montserrat_24, 0xf6f7fa);
    lv_obj_t *plus = surface(card, 196, 149, 112, 52, 0x20493f, LV_RADIUS_CIRCLE);
    make_button(plus, SMART_OS_UI_ACTION_BRIGHTEN);
    label(plus, LV_SYMBOL_PLUS, 0, 12, 112, &lv_font_montserrat_24, 0x9bf5d7);
    s_storage = label(s_settings, "", 40, 392, 330, &lv_font_montserrat_12, 0x7d8998);
    lv_obj_t *back = label(s_settings, LV_SYMBOL_LEFT "  Apps", 105, 423, 200, &lv_font_montserrat_14, 0x75e6c5);
    lv_obj_set_height(back, 26);
    make_button(back, SMART_OS_UI_ACTION_BUTTON);
}

esp_err_t smart_os_native_ui_show_settings(uint8_t brightness, bool sd_available)
{
    if (!smart_os_board_lock(200)) {
        return ESP_ERR_TIMEOUT;
    }
    ensure_settings();
    lv_label_set_text_fmt(s_brightness, "%u%%", (unsigned)brightness);
    lv_bar_set_value(s_brightness_bar, brightness, LV_ANIM_OFF);
    lv_label_set_text(s_storage, sd_available ? "SD card connected" : "SD card unavailable");
    if (lv_screen_active() != s_settings) {
        lv_screen_load(s_settings);
    }
    smart_os_board_unlock();
    ESP_LOGI("smart_os_ui", "settings-ui rendered brightness=%u", (unsigned)brightness);
    return ESP_OK;
}

esp_err_t smart_os_native_ui_show_app_error(const char *title)
{
    if (!smart_os_board_lock(200)) {
        return ESP_ERR_TIMEOUT;
    }
    if (s_error == NULL) {
        s_error = screen();
        s_error_title = label(s_error, "", 40, 114, 330, &lv_font_montserrat_32, 0xf6f7fa);
        label(s_error, "Could not open this app.\nCheck its installation on the SD card\nand try again.",
              40, 200, 330, &lv_font_montserrat_14, 0xa4b2c4);
        lv_obj_t *back = surface(s_error, 105, 325, 200, 56, 0x20493f, LV_RADIUS_CIRCLE);
        label(back, LV_SYMBOL_LEFT "  Apps", 0, 17, 200, &lv_font_montserrat_18, 0x9bf5d7);
        make_button(back, SMART_OS_UI_ACTION_BUTTON);
    }
    lv_label_set_text(s_error_title, title);
    lv_screen_load(s_error);
    smart_os_board_unlock();
    ESP_LOGW("smart_os_ui", "app-error rendered app=%s", title);
    return ESP_OK;
}

static lv_point_t s_tap;
static bool s_pressed;
static void read_test_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point = s_tap;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

esp_err_t smart_os_native_ui_inject_tap(int x, int y)
{
    if (x < 0 || x >= 410 || y < 0 || y >= 502) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!smart_os_board_lock(500)) {
        return ESP_ERR_TIMEOUT;
    }
    // Use LVGL's pointer hit testing and the same events as the real touch panel.
    lv_indev_t *pointer = lv_indev_create();
    if (pointer == NULL) {
        smart_os_board_unlock();
        return ESP_ERR_NO_MEM;
    }
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_mode(pointer, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(pointer, read_test_pointer);
    lv_indev_set_display(pointer, smart_os_board_display());
    s_tap = (lv_point_t){x, y};
    s_pressed = true;
    lv_indev_read(pointer);
    s_pressed = false;
    lv_indev_read(pointer);
    lv_indev_delete(pointer);
    smart_os_board_unlock();
    return ESP_OK;
}

esp_err_t smart_os_native_ui_snapshot(void)
{
    if (!smart_os_board_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_draw_buf_t *frame = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    smart_os_board_unlock();
    if (frame == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (frame->header.w > 410) {
        lv_draw_buf_destroy(frame);
        return ESP_ERR_INVALID_SIZE;
    }
    printf("WATCH-UI FRAME %lu %lu\n", (unsigned long)frame->header.w, (unsigned long)frame->header.h);
    for (uint32_t y = 0; y < frame->header.h; ++y) {
        const uint8_t *row = frame->data + y * frame->header.stride;
        // One printf per row prevents concurrent runtime logs splitting pixels.
        char hex[410 * 4 + 1];
        static const char digits[] = "0123456789abcdef";
        for (uint32_t x = 0; x < frame->header.w * 2; ++x) {
            hex[x * 2] = digits[row[x] >> 4];
            hex[x * 2 + 1] = digits[row[x] & 15];
        }
        hex[frame->header.w * 4] = '\0';
        printf("WATCH-UI PIXELS %s\n", hex);
        // A full frame is ~830 KB of hex. Let idle run during USB streaming.
        if ((y & 3) == 3) {
            vTaskDelay(1);
        }
    }
    printf("WATCH-UI FRAME-END\n");
    fflush(stdout);
    lv_draw_buf_destroy(frame);
    return ESP_OK;
}
