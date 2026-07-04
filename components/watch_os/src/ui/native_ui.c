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

static lv_obj_t *s_shell_screen;
static lv_obj_t *s_shell_title;
static lv_obj_t *s_shell_headline;
static lv_obj_t *s_shell_memory;
static lv_obj_t *s_shell_apps;
static lv_obj_t *s_shell_footer;

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

static void ensure_shell_screen(void)
{
    if (s_shell_screen != NULL) {
        return;
    }

    s_shell_screen = lv_obj_create(NULL);
    style_screen(s_shell_screen);

    s_shell_title = lv_label_create(s_shell_screen);
    lv_label_set_text(s_shell_title, "smartesp");
    lv_obj_set_style_text_font(s_shell_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_shell_title, lv_color_hex(0x68f0d5), 0);
    lv_obj_set_style_text_align(s_shell_title, LV_TEXT_ALIGN_LEFT, 0);

    s_shell_headline = lv_label_create(s_shell_screen);
    lv_label_set_long_mode(s_shell_headline, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_headline, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_shell_headline, LV_TEXT_ALIGN_LEFT, 0);

    s_shell_memory = lv_label_create(s_shell_screen);
    lv_label_set_long_mode(s_shell_memory, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_memory, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_shell_memory, lv_color_hex(0xfde68a), 0);
    lv_obj_set_style_text_align(s_shell_memory, LV_TEXT_ALIGN_LEFT, 0);

    s_shell_apps = lv_label_create(s_shell_screen);
    lv_label_set_long_mode(s_shell_apps, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_apps, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(s_shell_apps, LV_TEXT_ALIGN_LEFT, 0);

    s_shell_footer = lv_label_create(s_shell_screen);
    lv_label_set_long_mode(s_shell_footer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_shell_footer, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(s_shell_footer, &lv_font_montserrat_12, 0);
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

static void place_top_label_left(lv_obj_t *obj, lv_coord_t y, lv_coord_t base_padding)
{
    watch_ui_safe_zone_t zone;

    lv_obj_set_width(obj, LV_SIZE_CONTENT);
    lv_obj_update_layout(obj);
    zone = safe_zone_for_band(y, lv_obj_get_height(obj), base_padding);
    lv_obj_set_width(obj, zone.width);
    lv_obj_set_pos(obj, zone.left, y);
    lv_obj_update_layout(obj);
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

// Anchor a wrapping label above the bottom edge, leaving `bottom_margin` of
// clearance, and inset its sides for the rounded bottom corners. The label is
// measured twice because wrapping to the safe width can add lines and change
// the height that drives the final vertical position.
static void place_bottom_label_left(lv_obj_t *obj, lv_coord_t bottom_margin, lv_coord_t base_padding)
{
    lv_coord_t width = 410;
    lv_coord_t height_px = 502;
    watch_ui_safe_zone_t zone;
    lv_coord_t obj_height;
    lv_coord_t y;

    screen_size(&width, &height_px);

    lv_obj_set_width(obj, LV_SIZE_CONTENT);
    lv_obj_update_layout(obj);

    for (int pass = 0; pass < 2; ++pass) {
        obj_height = lv_obj_get_height(obj);
        y = height_px - bottom_margin - obj_height;
        zone = safe_zone_for_band(y, obj_height, base_padding);
        lv_obj_set_width(obj, zone.width);
        lv_obj_update_layout(obj);
    }

    obj_height = lv_obj_get_height(obj);
    y = height_px - bottom_margin - obj_height;
    lv_obj_set_pos(obj, zone.left, y);
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

static void layout_shell_screen(void)
{
    lv_coord_t y = 18;

    place_top_label_left(s_shell_title, y, 20);
    y += lv_obj_get_height(s_shell_title) + 12;

    place_top_label_left(s_shell_headline, y, 20);
    y += lv_obj_get_height(s_shell_headline) + 14;

    place_top_label_left(s_shell_memory, y, 20);
    y += lv_obj_get_height(s_shell_memory) + 14;

    place_top_label_left(s_shell_apps, y, 20);

    place_bottom_label_left(s_shell_footer, 22, 20);
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
    ESP_LOGI(TAG, "boot-ui stage=\"%s\" progress=%u", (stage != NULL) ? stage : "booting", (unsigned)progress);

    watch_board_unlock();
    return ESP_OK;
}

esp_err_t watch_native_ui_show_shell(const watch_ui_shell_model_t *model)
{
    if (model == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_board_lock(200)) {
        ESP_LOGW(TAG, "display lock timeout during shell UI");
        return ESP_ERR_TIMEOUT;
    }

    ensure_shell_screen();
    lv_label_set_text(s_shell_headline, model->headline);
    lv_label_set_text(s_shell_memory, model->memory_line);
    lv_label_set_text(s_shell_apps, model->apps_line);
    lv_label_set_text(s_shell_footer, model->footer);
    layout_shell_screen();
    lv_screen_load(s_shell_screen);
    ESP_LOGI(TAG, "shell-ui rendered");

    watch_board_unlock();
    return ESP_OK;
}
