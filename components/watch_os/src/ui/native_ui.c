#include "native_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board/watch_board.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "watch_ui";

static lv_obj_t *s_boot_screen;
static lv_obj_t *s_boot_title;
static lv_obj_t *s_boot_stage;
static lv_obj_t *s_boot_bar;

static lv_obj_t *s_clock_screen;
static lv_obj_t *s_clock_halo;
static lv_obj_t *s_clock_badge;
static lv_obj_t *s_clock_badge_label;
static lv_obj_t *s_clock_time;
static lv_obj_t *s_clock_seconds;
static lv_obj_t *s_clock_date;
static lv_obj_t *s_clock_rule;
static lv_obj_t *s_clock_app_card;
static lv_obj_t *s_clock_app_title;
static lv_obj_t *s_clock_app_caption;
static lv_obj_t *s_clock_brand;

static void open_apps(lv_event_t *event)
{
    (void)event;
    watch_native_ui_request(WATCH_UI_ACTION_BUTTON);
}

typedef struct {
    lv_coord_t left;
    lv_coord_t width;
} watch_ui_safe_zone_t;

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x09090b), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xf5f5f5), 0);
}

static void ensure_boot_screen(void)
{
    if (s_boot_screen != NULL) {
        return;
    }

    s_boot_screen = lv_obj_create(NULL);
    style_screen(s_boot_screen);

    s_boot_title = lv_label_create(s_boot_screen);
    lv_label_set_text(s_boot_title, "smartesp");
    lv_obj_set_style_text_font(s_boot_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_boot_title, lv_color_hex(0x68f0d5), 0);
    lv_obj_set_style_text_align(s_boot_title, LV_TEXT_ALIGN_CENTER, 0);

    s_boot_stage = lv_label_create(s_boot_screen);
    lv_label_set_text(s_boot_stage, "preparing watch shell");
    lv_obj_set_style_text_font(s_boot_stage, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_boot_stage, LV_TEXT_ALIGN_CENTER, 0);

    s_boot_bar = lv_bar_create(s_boot_screen);
    lv_obj_set_size(s_boot_bar, 220, 12);
    lv_obj_set_style_bg_color(s_boot_bar, lv_color_hex(0x1f2937), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_boot_bar, lv_color_hex(0xf97316), LV_PART_INDICATOR);
    lv_bar_set_range(s_boot_bar, 0, 100);
}

static void clear_object_chrome(lv_obj_t *obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
}

static void ensure_clock_screen(void)
{
    if (s_clock_screen != NULL) {
        return;
    }

    s_clock_screen = lv_obj_create(NULL);
    style_screen(s_clock_screen);

    s_clock_halo = lv_obj_create(s_clock_screen);
    clear_object_chrome(s_clock_halo);
    lv_obj_set_size(s_clock_halo, 330, 330);
    lv_obj_set_style_radius(s_clock_halo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_clock_halo, lv_color_hex(0x101b24), 0);
    lv_obj_set_style_bg_opa(s_clock_halo, LV_OPA_50, 0);
    lv_obj_align(s_clock_halo, LV_ALIGN_CENTER, 0, -38);
    lv_obj_remove_flag(s_clock_halo, LV_OBJ_FLAG_CLICKABLE);

    s_clock_badge = lv_obj_create(s_clock_screen);
    clear_object_chrome(s_clock_badge);
    lv_obj_set_size(s_clock_badge, 154, 30);
    lv_obj_set_style_radius(s_clock_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_clock_badge, lv_color_hex(0x13342f), 0);
    lv_obj_set_style_bg_opa(s_clock_badge, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_clock_badge, 128, 58);
    lv_obj_remove_flag(s_clock_badge, LV_OBJ_FLAG_CLICKABLE);

    s_clock_badge_label = lv_label_create(s_clock_badge);
    lv_label_set_text(s_clock_badge_label, "WATCH");
    lv_obj_set_style_text_font(s_clock_badge_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_clock_badge_label, lv_color_hex(0x75f3d5), 0);
    lv_obj_center(s_clock_badge_label);

    s_clock_time = lv_label_create(s_clock_screen);
    lv_obj_set_style_text_font(s_clock_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_clock_time, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_style_text_letter_space(s_clock_time, 2, 0);

    s_clock_seconds = lv_label_create(s_clock_screen);
    lv_obj_set_style_text_font(s_clock_seconds, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(s_clock_seconds, lv_color_hex(0x68f0d5), 0);

    s_clock_rule = lv_obj_create(s_clock_screen);
    clear_object_chrome(s_clock_rule);
    lv_obj_set_size(s_clock_rule, 44, 3);
    lv_obj_set_style_radius(s_clock_rule, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_clock_rule, lv_color_hex(0xf97316), 0);
    lv_obj_set_style_bg_opa(s_clock_rule, LV_OPA_COVER, 0);

    s_clock_date = lv_label_create(s_clock_screen);
    lv_obj_set_width(s_clock_date, 300);
    lv_obj_set_style_text_font(s_clock_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_clock_date, lv_color_hex(0xb6c2cf), 0);
    lv_obj_set_style_text_align(s_clock_date, LV_TEXT_ALIGN_CENTER, 0);

    s_clock_app_card = lv_obj_create(s_clock_screen);
    lv_obj_set_size(s_clock_app_card, 300, 74);
    lv_obj_set_style_radius(s_clock_app_card, 24, 0);
    lv_obj_set_style_bg_color(s_clock_app_card, lv_color_hex(0x111820), 0);
    lv_obj_set_style_bg_opa(s_clock_app_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_clock_app_card, lv_color_hex(0x263542), 0);
    lv_obj_set_style_border_width(s_clock_app_card, 1, 0);
    lv_obj_set_style_pad_all(s_clock_app_card, 0, 0);
    lv_obj_add_flag(s_clock_app_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_clock_app_card, open_apps, LV_EVENT_CLICKED, NULL);

    s_clock_app_title = lv_label_create(s_clock_app_card);
    lv_label_set_text(s_clock_app_title, "YOUR APPS");
    lv_obj_set_style_text_font(s_clock_app_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_clock_app_title, lv_color_hex(0xf8fafc), 0);
    lv_obj_set_pos(s_clock_app_title, 22, 16);

    s_clock_app_caption = lv_label_create(s_clock_app_card);
    lv_label_set_text(s_clock_app_caption, "Tap here or press the side button");
    lv_obj_set_style_text_font(s_clock_app_caption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_clock_app_caption, lv_color_hex(0x7f91a3), 0);
    lv_obj_set_pos(s_clock_app_caption, 22, 40);

    s_clock_brand = lv_label_create(s_clock_screen);
    lv_label_set_text(s_clock_brand, "smartesp");
    lv_obj_set_style_text_font(s_clock_brand, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_clock_brand, lv_color_hex(0x536270), 0);
}

static void screen_size(lv_coord_t *width_out, lv_coord_t *height_out)
{
    lv_display_t *display = watch_board_display();

    if (display == NULL) {
        if (width_out != NULL) {
            *width_out = 410;
        }
        if (height_out != NULL) {
            *height_out = 502;
        }
        return;
    }

    if (width_out != NULL) {
        *width_out = (lv_coord_t)lv_display_get_horizontal_resolution(display);
    }
    if (height_out != NULL) {
        *height_out = (lv_coord_t)lv_display_get_vertical_resolution(display);
    }
}

// Horizontal distance a rounded corner of `radius` eats into the screen at a
// point `edge_dist` pixels away from the nearest straight (top or bottom) edge.
// Zero once we are past the curved region.
static lv_coord_t corner_inset_for_edge_distance(lv_coord_t edge_dist, float radius)
{
    if (edge_dist < 0) {
        edge_dist = 0;
    }
    if ((float)edge_dist >= radius) {
        return 0;
    }

    const float offset = radius - (float)edge_dist;
    const float curve = radius - sqrtf((radius * radius) - (offset * offset));
    return (lv_coord_t)(curve + 0.5f);
}

// Compute the horizontal safe zone for a band spanning [y, y + height]. The
// padding grows dynamically with how far the band reaches into the rounded top
// or bottom corners, using the worst-case (deepest) point of the band on each
// side so no glyph on the first or last line is clipped.
static watch_ui_safe_zone_t safe_zone_for_band(lv_coord_t y, lv_coord_t height, lv_coord_t base_padding)
{
    lv_coord_t width = 410;
    lv_coord_t height_px = 502;
    const lv_coord_t min_content_width = 180;

    screen_size(&width, &height_px);

    if (height < 0) {
        height = 0;
    }

    const float radius = (float)LV_MIN(width / 4, height_px / 5);
    const lv_coord_t band_top = y;
    const lv_coord_t band_bottom = y + height;

    const lv_coord_t top_inset = corner_inset_for_edge_distance(band_top, radius);
    const lv_coord_t bottom_inset = corner_inset_for_edge_distance(height_px - band_bottom, radius);
    lv_coord_t inset = base_padding + LV_MAX(top_inset, bottom_inset);

    if ((inset * 2) > (width - min_content_width)) {
        inset = (width - min_content_width) / 2;
    }

    watch_ui_safe_zone_t zone = {
        .left = inset,
        .width = width - (inset * 2),
    };
    return zone;
}

static void place_top_label_centered(lv_obj_t *obj, lv_coord_t y, lv_coord_t base_padding)
{
    watch_ui_safe_zone_t zone;

    lv_obj_set_width(obj, LV_SIZE_CONTENT);
    lv_obj_update_layout(obj);
    zone = safe_zone_for_band(y, lv_obj_get_height(obj), base_padding);
    lv_obj_set_width(obj, zone.width);
    lv_obj_set_pos(obj, zone.left, y);
    lv_obj_update_layout(obj);
}

static void place_top_bar_centered(lv_obj_t *obj, lv_coord_t y, lv_coord_t width_hint, lv_coord_t base_padding)
{
    watch_ui_safe_zone_t zone = safe_zone_for_band(y, lv_obj_get_height(obj), base_padding);
    const lv_coord_t width = LV_MIN(width_hint, zone.width);
    const lv_coord_t left = zone.left + ((zone.width - width) / 2);

    lv_obj_set_size(obj, width, lv_obj_get_height(obj));
    lv_obj_set_pos(obj, left, y);
}

static void layout_boot_screen(void)
{
    const lv_coord_t title_y = 62;
    lv_coord_t stage_y;
    lv_coord_t bar_y;

    place_top_label_centered(s_boot_title, title_y, 22);
    stage_y = title_y + lv_obj_get_height(s_boot_title) + 18;
    place_top_label_centered(s_boot_stage, stage_y, 22);
    bar_y = stage_y + lv_obj_get_height(s_boot_stage) + 24;
    place_top_bar_centered(s_boot_bar, bar_y, 220, 22);
}

static void layout_clock_screen(void)
{
    lv_obj_align(s_clock_time, LV_ALIGN_TOP_MID, -10, 150);
    lv_obj_align_to(s_clock_seconds, s_clock_time, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -6);
    lv_obj_align(s_clock_rule, LV_ALIGN_TOP_MID, 0, 229);
    lv_obj_align(s_clock_date, LV_ALIGN_TOP_MID, 0, 252);
    lv_obj_align(s_clock_app_card, LV_ALIGN_TOP_MID, 0, 315);
    lv_obj_align(s_clock_brand, LV_ALIGN_TOP_MID, 0, 430);
}

esp_err_t watch_native_ui_show_boot(const char *stage, uint8_t progress)
{
    if (!watch_board_lock(200)) {
        ESP_LOGW(TAG, "display lock timeout during boot UI");
        return ESP_ERR_TIMEOUT;
    }

    ensure_boot_screen();
    lv_label_set_text(s_boot_stage, (stage != NULL) ? stage : "booting");
    lv_bar_set_value(s_boot_bar, progress, LV_ANIM_ON);
    layout_boot_screen();
    lv_screen_load(s_boot_screen);
    lv_obj_invalidate(s_boot_screen);
    const esp_err_t present_err = watch_board_present_first_frame();
    if (present_err != ESP_OK) {
        watch_board_unlock();
        return present_err;
    }
    ESP_LOGI(TAG, "boot-ui stage=\"%s\" progress=%u", (stage != NULL) ? stage : "booting", (unsigned)progress);

    watch_board_unlock();
    return ESP_OK;
}

esp_err_t watch_native_ui_show_clock(const watch_ui_clock_model_t *model)
{
    if (model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_board_lock(200)) {
        ESP_LOGW(TAG, "display lock timeout during clock UI");
        return ESP_ERR_TIMEOUT;
    }

    ensure_clock_screen();
    lv_label_set_text(s_clock_time, model->hour_minute);
    lv_label_set_text(s_clock_seconds, model->seconds);
    lv_label_set_text(s_clock_date, model->date_line);
    layout_clock_screen();
    if (lv_screen_active() != s_clock_screen) {
        lv_screen_load(s_clock_screen);
    }
    ESP_LOGI(TAG, "clock-ui rendered time=%s:%s", model->hour_minute, model->seconds);

    watch_board_unlock();
    return ESP_OK;
}
