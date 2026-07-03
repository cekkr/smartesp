#include "watch_os.h"

#include <stdio.h>
#include <string.h>

#include "app_catalog.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "native_ui.h"
#include "process_manager.h"
#include "storage.h"
#include "wasm_runtime.h"
#include "watch_board.h"

typedef struct {
    watch_os_config_t config;
    watch_storage_t storage;
    watch_wasm_runtime_t wasm_runtime;
    watch_process_manager_t process_manager;
    TaskHandle_t maintenance_task;
} watch_os_state_t;

static const char *TAG = "watch_os";
static watch_os_state_t s_state;

static void update_shell_ui(void)
{
    watch_ui_shell_model_t model = {0};
    size_t internal_free = 0;
    size_t psram_free = 0;
    size_t used = 0;
    const bool pressure = watch_process_manager_memory_pressure(&s_state.process_manager, &internal_free, &psram_free);

    snprintf(model.headline,
             sizeof(model.headline),
             "native C owns boot/shared UI; WASM apps run through fayasm%s",
             watch_storage_available(&s_state.storage) ? " with SD hibernation" : " without SD hibernation");
    snprintf(model.memory_line,
             sizeof(model.memory_line),
             "RAM %u KiB | PSRAM %u KiB | pressure %s",
             (unsigned)(internal_free / 1024),
             (unsigned)(psram_free / 1024),
             pressure ? "yes" : "no");
    snprintf(model.footer,
             sizeof(model.footer),
             "storage: %s",
             watch_storage_available(&s_state.storage) ? watch_storage_root(&s_state.storage) : "not mounted");

    used += snprintf(model.apps_line + used,
                     sizeof(model.apps_line) - used,
                     "apps:\n");

    for (size_t i = 0; i < watch_process_manager_count(&s_state.process_manager) && used + 32 < sizeof(model.apps_line); ++i) {
        const watch_os_process_snapshot_t *process = watch_process_manager_at(&s_state.process_manager, i);
        if ((process == NULL) || (process->app == NULL)) {
            continue;
        }

        used += snprintf(model.apps_line + used,
                         sizeof(model.apps_line) - used,
                         "- %s [%s/%s]%s\n",
                         process->app->title,
                         watch_process_state_label(process->state),
                         watch_residency_label(process->app->residency),
                         process->resident_in_ram ? " resident" : "");

        if (process->has_last_result && used + 24 < sizeof(model.apps_line)) {
            used += snprintf(model.apps_line + used,
                             sizeof(model.apps_line) - used,
                             "  last i32 result: %ld\n",
                             (long)process->last_result_i32);
        }
    }

    (void)watch_native_ui_show_shell(&model);
}

static esp_err_t launch_demo_app_if_enabled(void)
{
    watch_os_process_snapshot_t *process;
    watch_wasm_result_t result = {0};
    esp_err_t err;

    if (!s_state.config.auto_launch_demo_app) {
        return ESP_OK;
    }

    process = watch_process_manager_find(&s_state.process_manager, "wasm.demo.add");
    if (process == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    watch_process_manager_mark_state(process, WATCH_PROCESS_RUNNING, true);
    err = watch_wasm_runtime_run(&s_state.wasm_runtime, process->app, &result);
    watch_process_manager_set_result(process,
                                     (err == ESP_OK) ? result.last_status : err,
                                     result.has_last_result,
                                     result.last_result_i32);
    if (err == ESP_OK && result.has_last_result) {
        ESP_LOGI(TAG, "demo app %s result=%ld", process->app->id, (long)result.last_result_i32);
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "demo app %s executed without i32 result", process->app->id);
    } else {
        ESP_LOGW(TAG, "demo app %s failed err=%s", process->app->id, esp_err_to_name(err));
    }
    watch_process_manager_mark_state(process,
                                     (err == ESP_OK) ? WATCH_PROCESS_READY : WATCH_PROCESS_FAULTED,
                                     result.loaded && !result.hibernated);
    return err;
}

static void handle_memory_pressure(void)
{
    watch_os_process_snapshot_t *victim;
    watch_wasm_result_t result = {0};

    if (!watch_process_manager_memory_pressure(&s_state.process_manager, NULL, NULL)) {
        return;
    }

    victim = watch_process_manager_pick_victim(&s_state.process_manager);
    if ((victim == NULL) || (victim->app == NULL)) {
        return;
    }

    if (victim->app->residency == WATCH_APP_RESIDENCY_HIBERNABLE) {
        if (watch_wasm_runtime_hibernate(&s_state.wasm_runtime, victim->app, &result) == ESP_OK) {
            watch_process_manager_set_result(victim, result.last_status, result.has_last_result, result.last_result_i32);
            watch_process_manager_mark_state(victim, WATCH_PROCESS_HIBERNATED, false);
        }
    } else if (victim->app->residency == WATCH_APP_RESIDENCY_DISPOSABLE) {
        (void)watch_wasm_runtime_unload(&s_state.wasm_runtime, victim->app);
        watch_process_manager_mark_state(victim, WATCH_PROCESS_STOPPED, false);
    }
}

static void maintenance_task(void *arg)
{
    (void)arg;

    while (true) {
        handle_memory_pressure();
        update_shell_ui();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t watch_os_start(const watch_os_config_t *config)
{
    size_t app_count = 0;
    const watch_os_app_descriptor_t *catalog;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "missing config");
    memset(&s_state, 0, sizeof(s_state));
    s_state.config = *config;

    ESP_RETURN_ON_ERROR(watch_board_init(), TAG, "board init failed");
    ESP_RETURN_ON_ERROR(watch_native_ui_show_boot("display ready", 10), TAG, "boot ui failed");

    ESP_RETURN_ON_ERROR(watch_native_ui_show_boot("probing storage", 35), TAG, "boot ui failed");
    (void)watch_storage_init(&s_state.storage, s_state.config.prefer_sdcard_runtime);

    ESP_RETURN_ON_ERROR(watch_native_ui_show_boot("preparing runtime", 55), TAG, "boot ui failed");
    ESP_RETURN_ON_ERROR(watch_wasm_runtime_init(&s_state.wasm_runtime, &s_state.storage), TAG, "runtime init failed");

    catalog = watch_app_catalog_get(&app_count);
    watch_process_manager_init(&s_state.process_manager,
                               s_state.config.internal_ram_floor_bytes,
                               s_state.config.psram_floor_bytes);
    watch_process_manager_register_catalog(&s_state.process_manager, catalog, app_count);

    ESP_RETURN_ON_ERROR(watch_native_ui_show_boot("registering apps", 80), TAG, "boot ui failed");
    (void)launch_demo_app_if_enabled();

    ESP_RETURN_ON_ERROR(watch_native_ui_show_boot("entering shell", 100), TAG, "boot ui failed");
    update_shell_ui();

    if (s_state.maintenance_task == NULL) {
        BaseType_t created = xTaskCreatePinnedToCore(maintenance_task,
                                                     "watch_os_maint",
                                                     6144,
                                                     NULL,
                                                     4,
                                                     &s_state.maintenance_task,
                                                     tskNO_AFFINITY);
        ESP_RETURN_ON_FALSE(created == pdPASS, ESP_FAIL, TAG, "maintenance task create failed");
    }

    ESP_LOGI(TAG, "smartesp started");
    return ESP_OK;
}
