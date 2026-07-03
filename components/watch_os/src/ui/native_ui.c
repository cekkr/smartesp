#include "native_ui.h"

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
    lv_obj_align(s_boot_title, LV_ALIGN_TOP_MID, 0, 70);

    s_boot_stage = lv_label_create(s_boot_screen);
    lv_label_set_text(s_boot_stage, "preparing watch shell");
    lv_obj_set_style_text_font(s_boot_stage, &lv_font_montserrat_14, 0);
    lv_obj_align_to(s_boot_stage, s_boot_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 18);

    s_boot_bar = lv_bar_create(s_boot_screen);
    lv_obj_set_size(s_boot_bar, 220, 12);
    lv_obj_set_style_bg_color(s_boot_bar, lv_color_hex(0x1f2937), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_boot_bar, lv_color_hex(0xf97316), LV_PART_INDICATOR);
    lv_obj_align_to(s_boot_bar, s_boot_stage, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);
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
    lv_obj_align(s_shell_title, LV_ALIGN_TOP_LEFT, 20, 18);

    s_shell_headline = lv_label_create(s_shell_screen);
    lv_obj_set_width(s_shell_headline, 360);
    lv_label_set_long_mode(s_shell_headline, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_headline, &lv_font_montserrat_14, 0);
    lv_obj_align_to(s_shell_headline, s_shell_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);

    s_shell_memory = lv_label_create(s_shell_screen);
    lv_obj_set_width(s_shell_memory, 360);
    lv_label_set_long_mode(s_shell_memory, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_memory, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_shell_memory, lv_color_hex(0xfde68a), 0);
    lv_obj_align_to(s_shell_memory, s_shell_headline, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 14);

    s_shell_apps = lv_label_create(s_shell_screen);
    lv_obj_set_width(s_shell_apps, 370);
    lv_label_set_long_mode(s_shell_apps, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_shell_apps, &lv_font_montserrat_12, 0);
    lv_obj_align_to(s_shell_apps, s_shell_memory, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 14);

    s_shell_footer = lv_label_create(s_shell_screen);
    lv_obj_set_width(s_shell_footer, 360);
    lv_label_set_long_mode(s_shell_footer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_shell_footer, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(s_shell_footer, &lv_font_montserrat_12, 0);
    lv_obj_align(s_shell_footer, LV_ALIGN_BOTTOM_LEFT, 20, -22);
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
    lv_screen_load(s_boot_screen);

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
    lv_screen_load(s_shell_screen);

    watch_board_unlock();
    return ESP_OK;
}
