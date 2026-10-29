#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "fa_job.h"
#include "fa_runtime.h"
#include "fa_wasm.h"
#include "storage.h"
#include "smart_os_app.h"

#define SMART_OS_WASM_MAX_CONTEXTS 4

typedef struct {
    const smart_os_app_descriptor_t *app;
    const smart_os_storage_t *storage;
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
} smart_os_wasm_context_t;

typedef struct {
    const smart_os_storage_t *storage;
    smart_os_wasm_context_t contexts[SMART_OS_WASM_MAX_CONTEXTS];
} smart_os_wasm_runtime_t;

typedef struct {
    bool loaded;
    bool hibernated;
    bool has_last_result;
    int32_t last_result_i32;
    int last_status;
} smart_os_wasm_result_t;

esp_err_t smart_os_wasm_runtime_init(smart_os_wasm_runtime_t *runtime, const smart_os_storage_t *storage);
esp_err_t smart_os_wasm_runtime_run(smart_os_wasm_runtime_t *runtime,
                                 const smart_os_app_descriptor_t *app,
                                 smart_os_wasm_result_t *result);
esp_err_t smart_os_wasm_runtime_run_with_input(smart_os_wasm_runtime_t *runtime,
                                            const smart_os_app_descriptor_t *app,
                                            int32_t action, int32_t value,
                                            smart_os_wasm_result_t *result);
esp_err_t smart_os_wasm_runtime_hibernate(smart_os_wasm_runtime_t *runtime,
                                       const smart_os_app_descriptor_t *app,
                                       smart_os_wasm_result_t *result);
esp_err_t smart_os_wasm_runtime_resume(smart_os_wasm_runtime_t *runtime,
                                    const smart_os_app_descriptor_t *app,
                                    smart_os_wasm_result_t *result);
esp_err_t smart_os_wasm_runtime_unload(smart_os_wasm_runtime_t *runtime, const smart_os_app_descriptor_t *app);
