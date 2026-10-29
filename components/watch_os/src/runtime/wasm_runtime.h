#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "fa_job.h"
#include "fa_runtime.h"
#include "fa_wasm.h"
#include "storage.h"
#include "watch_os_app.h"

#define WATCH_WASM_MAX_CONTEXTS 4

typedef struct {
    const watch_os_app_descriptor_t *app;
    const watch_storage_t *storage;
    WasmModule *module;
    fa_Runtime *runtime;
    fa_Job *job;
    uint32_t entry_function_index;
    bool loaded;
    bool hibernated;
    bool has_last_result;
    int32_t last_result_i32;
    int last_status;
    int32_t input_action;
    int32_t input_value;
} watch_wasm_context_t;

typedef struct {
    const watch_storage_t *storage;
    watch_wasm_context_t contexts[WATCH_WASM_MAX_CONTEXTS];
} watch_wasm_runtime_t;

typedef struct {
    bool loaded;
    bool hibernated;
    bool has_last_result;
    int32_t last_result_i32;
    int last_status;
} watch_wasm_result_t;

esp_err_t watch_wasm_runtime_init(watch_wasm_runtime_t *runtime, const watch_storage_t *storage);
esp_err_t watch_wasm_runtime_run(watch_wasm_runtime_t *runtime,
                                 const watch_os_app_descriptor_t *app,
                                 watch_wasm_result_t *result);
esp_err_t watch_wasm_runtime_run_with_input(watch_wasm_runtime_t *runtime,
                                            const watch_os_app_descriptor_t *app,
                                            int32_t action, int32_t value,
                                            watch_wasm_result_t *result);
esp_err_t watch_wasm_runtime_hibernate(watch_wasm_runtime_t *runtime,
                                       const watch_os_app_descriptor_t *app,
                                       watch_wasm_result_t *result);
esp_err_t watch_wasm_runtime_resume(watch_wasm_runtime_t *runtime,
                                    const watch_os_app_descriptor_t *app,
                                    watch_wasm_result_t *result);
esp_err_t watch_wasm_runtime_unload(watch_wasm_runtime_t *runtime, const watch_os_app_descriptor_t *app);
