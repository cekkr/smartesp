#include "smart_os.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>

#include "app_catalog.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "native_ui.h"
#include "process_manager.h"
#include "serial_uploader.h"
#include "storage.h"
#include "wasm_runtime.h"
#include "smart_os_board.h"

typedef enum { SMART_OS_VIEW_WATCH, SMART_OS_VIEW_APPS, SMART_OS_VIEW_SETTINGS, SMART_OS_VIEW_ERROR } smart_os_view_t;

typedef struct {
    smart_os_config_t config;
    smart_os_storage_t storage;
    smart_os_wasm_runtime_t wasm_runtime;
    smart_os_process_manager_t process_manager;
    TaskHandle_t maintenance_task;
    int64_t last_clock_tick_seconds;
    QueueHandle_t actions;
    smart_os_view_t view;
    uint8_t brightness;
} smart_os_state_t;

static const char *TAG = "smart_os";
static smart_os_state_t s_state;

static void seed_wall_clock_from_build(void)
{
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    struct timeval now;

    gettimeofday(&now, NULL);
    if (now.tv_sec >= 1704067200) {
        return;
    }

    struct tm build = {0};
    char month[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
    const char *month_pos = strstr(months, month);
    build.tm_mon = (month_pos != NULL) ? (int)((month_pos - months) / 3) : 0;
    build.tm_mday = (__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (((__DATE__[4] - '0') * 10) + (__DATE__[5] - '0'));
    build.tm_year = ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 +
                     (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0')) - 1900;
    build.tm_hour = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    build.tm_min = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    build.tm_sec = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
    build.tm_isdst = -1;

    now.tv_sec = mktime(&build);
    now.tv_usec = 0;
    if (now.tv_sec > 0) {
        settimeofday(&now, NULL);
        ESP_LOGW(TAG, "wall clock was unset; seeded from firmware build time");
    }
}

static void prepare_external_app_storage(const smart_os_app_descriptor_t *catalog, size_t count)
{
    if ((catalog == NULL) || !smart_os_storage_available(&s_state.storage)) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (catalog[i].storage_path == NULL) {
            continue;
        }

        if (smart_os_storage_prepare_external_wasm_path(&s_state.storage, catalog[i].storage_path) != ESP_OK) {
            ESP_LOGW(TAG, "could not prepare storage path for %s", catalog[i].id);
        }
    }
}

static bool format_clock_result(int32_t encoded, char *buffer, size_t buffer_size)
{
    const int32_t hours = encoded / 10000;
    const int32_t minutes = (encoded / 100) % 100;
    const int32_t seconds = encoded % 100;

    if ((buffer == NULL) || (buffer_size == 0) || (encoded < 0) ||
        (hours < 0) || (hours > 23) ||
        (minutes < 0) || (minutes > 59) ||
        (seconds < 0) || (seconds > 59)) {
        return false;
    }

    snprintf(buffer, buffer_size, "%02ld:%02ld:%02ld", (long)hours, (long)minutes, (long)seconds);
    return true;
}

static void uppercase_ascii(char *text)
{
    for (; (text != NULL) && (*text != '\0'); ++text) {
        *text = (char)toupper((unsigned char)*text);
    }
}

static void update_clock_ui(void)
{
    smart_os_ui_clock_model_t model = {0};
    char clock_buffer[16];
    const smart_os_process_snapshot_t *clock_process =
        smart_os_process_manager_find(&s_state.process_manager, "wasm.demo.clock");
    const time_t now = time(NULL);
    struct tm local_now = {0};

    if ((clock_process == NULL) || !clock_process->has_last_result ||
        !format_clock_result(clock_process->last_result_i32, clock_buffer, sizeof(clock_buffer))) {
        return;
    }

    memcpy(model.hour_minute, clock_buffer, 5);
    model.hour_minute[5] = '\0';
    memcpy(model.seconds, clock_buffer + 6, 2);
    model.seconds[2] = '\0';
    localtime_r(&now, &local_now);
    strftime(model.date_line, sizeof(model.date_line), "%A  /  %B %d", &local_now);
    uppercase_ascii(model.date_line);
    (void)smart_os_native_ui_show_clock(&model);
}

static esp_err_t tick_clock_demo_if_due(void)
{
    smart_os_process_snapshot_t *process;
    smart_os_wasm_result_t result = {0};
    char clock_buffer[16];
    const int64_t now_seconds = (int64_t)(esp_timer_get_time() / 1000000);
    esp_err_t err;

    if (s_state.last_clock_tick_seconds == now_seconds) {
        return ESP_OK;
    }

    process = smart_os_process_manager_find(&s_state.process_manager, "wasm.demo.clock");
    if (process == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    s_state.last_clock_tick_seconds = now_seconds;
    smart_os_process_manager_mark_state(process, SMART_OS_PROCESS_RUNNING, true);
    err = smart_os_wasm_runtime_run(&s_state.wasm_runtime, process->app, &result);
    smart_os_process_manager_set_result(process,
                                     (err == ESP_OK) ? result.last_status : err,
                                     result.has_last_result,
                                     result.last_result_i32);
    smart_os_process_manager_mark_state(process,
                                     (err == ESP_OK) ? SMART_OS_PROCESS_READY : SMART_OS_PROCESS_FAULTED,
                                     result.loaded && !result.hibernated);

    if ((err == ESP_OK) && result.has_last_result &&
        format_clock_result(result.last_result_i32, clock_buffer, sizeof(clock_buffer))) {
        ESP_LOGI(TAG,
                 "clock demo %s time=%s encoded=%ld",
                 process->app->id,
                 clock_buffer,
                 (long)result.last_result_i32);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "clock demo %s failed err=%s", process->app->id, esp_err_to_name(err));
    }

    return err;
}

static void handle_memory_pressure(void)
{
    smart_os_process_snapshot_t *victim;
    smart_os_wasm_result_t result = {0};

    if (!smart_os_process_manager_memory_pressure(&s_state.process_manager, NULL, NULL)) {
        return;
    }

    victim = smart_os_process_manager_pick_victim(&s_state.process_manager);
    if ((victim == NULL) || (victim->app == NULL)) {
        return;
    }

    if (victim->app->residency == SMART_OS_APP_RESIDENCY_HIBERNABLE) {
        if (smart_os_wasm_runtime_hibernate(&s_state.wasm_runtime, victim->app, &result) == ESP_OK) {
            smart_os_process_manager_set_result(victim, result.last_status, result.has_last_result, result.last_result_i32);
            smart_os_process_manager_mark_state(victim, SMART_OS_PROCESS_HIBERNATED, false);
        }
    } else if (victim->app->residency == SMART_OS_APP_RESIDENCY_DISPOSABLE) {
        (void)smart_os_wasm_runtime_unload(&s_state.wasm_runtime, victim->app);
        smart_os_process_manager_mark_state(victim, SMART_OS_PROCESS_STOPPED, false);
    }
}

static void request_action(smart_os_ui_action_t action)
{
    if (s_state.actions != NULL && xQueueSend(s_state.actions, &action, 0) != pdTRUE) {
        ESP_LOGW(TAG, "app action queue full");
    }
}

static void set_foreground(const char *id)
{
    for (size_t i = 0; i < s_state.process_manager.count; ++i) {
        smart_os_process_snapshot_t *process = &s_state.process_manager.slots[i];
        process->foreground = id != NULL && strcmp(process->app->id, id) == 0;
    }
}

static void read_brightness(void)
{
    nvs_handle_t handle;
    s_state.brightness = 80;
    if (nvs_open("watch_settings", NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u8(handle, "brightness", &s_state.brightness);
        nvs_close(handle);
    }
    if (s_state.brightness < 20 || s_state.brightness > 100) {
        s_state.brightness = 80;
    }
    (void)smart_os_board_set_brightness(s_state.brightness);
}

static void persist_brightness(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("watch_settings", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "brightness", s_state.brightness);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "brightness persistence failed: %s", esp_err_to_name(err));
    }
}

static void close_settings(void)
{
    smart_os_process_snapshot_t *process = smart_os_process_manager_find(&s_state.process_manager, "wasm.settings");
    if (process != NULL) {
        (void)smart_os_wasm_runtime_unload(&s_state.wasm_runtime, process->app);
        smart_os_process_manager_mark_state(process, SMART_OS_PROCESS_STOPPED, false);
    }
}

static void open_watch(void)
{
    s_state.view = SMART_OS_VIEW_WATCH;
    set_foreground("wasm.demo.clock");
    s_state.last_clock_tick_seconds = -1;
    if (tick_clock_demo_if_due() == ESP_OK) {
        update_clock_ui();
    } else {
        s_state.view = SMART_OS_VIEW_ERROR;
        set_foreground(NULL);
        (void)smart_os_native_ui_show_app_error("Watch");
    }
}

static void run_settings(int32_t action)
{
    smart_os_process_snapshot_t *process = smart_os_process_manager_find(&s_state.process_manager, "wasm.settings");
    smart_os_wasm_result_t result = {0};
    if (process == NULL) {
        return;
    }
    set_foreground(process->app->id);
    smart_os_process_manager_mark_state(process, SMART_OS_PROCESS_RUNNING, true);
    esp_err_t err = smart_os_wasm_runtime_run_with_input(&s_state.wasm_runtime, process->app, action,
                                                      s_state.brightness, &result);
    if (err == ESP_OK && (!result.has_last_result || result.last_result_i32 < 20 || result.last_result_i32 > 100)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    smart_os_process_manager_set_result(process, err, result.has_last_result, result.last_result_i32);
    smart_os_process_manager_mark_state(process, err == ESP_OK ? SMART_OS_PROCESS_READY : SMART_OS_PROCESS_FAULTED, result.loaded);
    if (err != ESP_OK) {
        s_state.view = SMART_OS_VIEW_ERROR;
        set_foreground(NULL);
        (void)smart_os_native_ui_show_app_error("Settings");
        return;
    }
    if (s_state.brightness != result.last_result_i32) {
        err = smart_os_board_set_brightness((uint8_t)result.last_result_i32);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "brightness apply failed: %s", esp_err_to_name(err));
            return;
        }
        s_state.brightness = (uint8_t)result.last_result_i32;
        persist_brightness();
    }
    s_state.view = SMART_OS_VIEW_SETTINGS;
    (void)smart_os_native_ui_show_settings(s_state.brightness, smart_os_storage_available(&s_state.storage));
}

static void handle_action(smart_os_ui_action_t action)
{
    ESP_LOGI(TAG, "app action=%d view=%d", (int)action, (int)s_state.view);
    switch (action) {
    case SMART_OS_UI_ACTION_BUTTON:
        if (s_state.view == SMART_OS_VIEW_APPS) {
            open_watch();
        } else {
            close_settings();
            set_foreground(NULL);
            s_state.view = SMART_OS_VIEW_APPS;
            (void)smart_os_native_ui_show_apps();
        }
        break;
    case SMART_OS_UI_ACTION_WATCH:
        close_settings();
        open_watch();
        break;
    case SMART_OS_UI_ACTION_SETTINGS:
        run_settings(0);
        break;
    case SMART_OS_UI_ACTION_DIM:
    case SMART_OS_UI_ACTION_BRIGHTEN:
        if (s_state.view == SMART_OS_VIEW_SETTINGS) {
            run_settings(action == SMART_OS_UI_ACTION_DIM ? -1 : 1);
        }
        break;
    }
}

static void maintenance_task(void *arg)
{
    (void)arg;
    while (true) {
        smart_os_ui_action_t action;
        if (xQueueReceive(s_state.actions, &action, pdMS_TO_TICKS(20)) == pdTRUE) {
            handle_action(action);
        }
        if (smart_os_board_poll_button()) {
            handle_action(SMART_OS_UI_ACTION_BUTTON);
        }
        const int64_t now_seconds = esp_timer_get_time() / 1000000;
        if (s_state.view == SMART_OS_VIEW_WATCH && s_state.last_clock_tick_seconds != now_seconds) {
            if (tick_clock_demo_if_due() == ESP_OK) {
                update_clock_ui();
            } else {
                s_state.view = SMART_OS_VIEW_ERROR;
                set_foreground(NULL);
                (void)smart_os_native_ui_show_app_error("Watch");
            }
        }
        handle_memory_pressure();
    }
}

esp_err_t smart_os_start(const smart_os_config_t *config)
{
    size_t app_count = 0;
    const smart_os_app_descriptor_t *catalog;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "missing config");
    memset(&s_state, 0, sizeof(s_state));
    s_state.config = *config;
    s_state.last_clock_tick_seconds = -1;
    s_state.actions = xQueueCreate(12, sizeof(smart_os_ui_action_t));
    ESP_RETURN_ON_FALSE(s_state.actions != NULL, ESP_ERR_NO_MEM, TAG, "action queue allocation failed");
    smart_os_native_ui_set_action_handler(request_action);
    seed_wall_clock_from_build();

    ESP_RETURN_ON_ERROR(smart_os_board_init(), TAG, "board init failed");
    ESP_RETURN_ON_ERROR(smart_os_native_ui_show_boot("display ready", 10), TAG, "boot ui failed");

    ESP_RETURN_ON_ERROR(smart_os_native_ui_show_boot("probing storage", 35), TAG, "boot ui failed");
    (void)smart_os_storage_init(&s_state.storage, s_state.config.prefer_sdcard_runtime);

    ESP_RETURN_ON_ERROR(smart_os_native_ui_show_boot("preparing runtime", 55), TAG, "boot ui failed");
    ESP_RETURN_ON_ERROR(smart_os_wasm_runtime_init(&s_state.wasm_runtime, &s_state.storage), TAG, "runtime init failed");

    catalog = smart_os_app_catalog_get(&app_count);
    smart_os_process_manager_init(&s_state.process_manager,
                               s_state.config.internal_ram_floor_bytes,
                               s_state.config.psram_floor_bytes);
    smart_os_process_manager_register_catalog(&s_state.process_manager, catalog, app_count);
    prepare_external_app_storage(catalog, app_count);

    ESP_RETURN_ON_ERROR(smart_os_native_ui_show_boot("registering apps", 80), TAG, "boot ui failed");
    read_brightness();

    ESP_RETURN_ON_ERROR(smart_os_native_ui_show_boot("launching clock", 100), TAG, "boot ui failed");
    if (s_state.config.auto_launch_demo_app) {
        open_watch();
    } else {
        s_state.view = SMART_OS_VIEW_APPS;
        (void)smart_os_native_ui_show_apps();
    }
    (void)smart_os_serial_uploader_start(&s_state.storage);

    if (s_state.maintenance_task == NULL) {
        BaseType_t created = xTaskCreatePinnedToCore(maintenance_task,
                                                     "smart_os_maint",
                                                     6144,
                                                     NULL,
                                                     4,
                                                     &s_state.maintenance_task,
                                                     tskNO_AFFINITY);
        ESP_RETURN_ON_FALSE(created == pdPASS, ESP_FAIL, TAG, "maintenance task create failed");
    }

    ESP_LOGI(TAG, "SmartOS started");
    return ESP_OK;
}
