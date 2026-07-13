#include "wasm_runtime.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "fa_job.h"
#include "fa_runtime.h"
#include "fa_wasm.h"
#include "fayasm_port.h"
#include "storage.h"

static const char *TAG = "watch_wasm";

static watch_wasm_context_t *find_context(watch_wasm_runtime_t *runtime, const watch_os_app_descriptor_t *app)
{
    for (size_t i = 0; i < WATCH_WASM_MAX_CONTEXTS; ++i) {
        if (runtime->contexts[i].app == app) {
            return &runtime->contexts[i];
        }
    }

    return NULL;
}

static watch_wasm_context_t *get_or_create_context(watch_wasm_runtime_t *runtime, const watch_os_app_descriptor_t *app)
{
    watch_wasm_context_t *free_slot = NULL;

    if ((runtime == NULL) || (app == NULL)) {
        return NULL;
    }

    for (size_t i = 0; i < WATCH_WASM_MAX_CONTEXTS; ++i) {
        if (runtime->contexts[i].app == app) {
            return &runtime->contexts[i];
        }
        if ((runtime->contexts[i].app == NULL) && (free_slot == NULL)) {
            free_slot = &runtime->contexts[i];
        }
    }

    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->app = app;
        free_slot->storage = runtime->storage;
    }

    return free_slot;
}

static void reset_context_slot(watch_wasm_runtime_t *runtime,
                               watch_wasm_context_t *ctx,
                               const watch_os_app_descriptor_t *app)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->storage = runtime->storage;
}

static void publish_result(const watch_wasm_context_t *ctx, watch_wasm_result_t *result)
{
    if ((ctx == NULL) || (result == NULL)) {
        return;
    }

    result->loaded = ctx->loaded;
    result->hibernated = ctx->hibernated;
    result->has_last_result = ctx->has_last_result;
    result->last_result_i32 = ctx->last_result_i32;
    result->last_status = ctx->last_status;
}

static int find_exported_function(const WasmModule *module, const char *name, uint32_t *out_index)
{
    if ((module == NULL) || (name == NULL) || (out_index == NULL) || (module->exports == NULL)) {
        return 0;
    }

    for (uint32_t i = 0; i < module->num_exports; ++i) {
        const WasmExport *export_entry = &module->exports[i];
        if ((export_entry->kind == 0) && (export_entry->name != NULL) &&
            (strcmp(export_entry->name, name) == 0)) {
            *out_index = export_entry->index;
            return 1;
        }
    }

    return 0;
}

static int host_add(fa_Runtime *runtime, const fa_RuntimeHostCall *call, void *user_data)
{
    i32 lhs = 0;
    i32 rhs = 0;

    (void)runtime;
    (void)user_data;

    if (!fa_RuntimeHostCall_expect(call, 2, 1) ||
        !fa_RuntimeHostCall_arg_i32(call, 0, &lhs) ||
        !fa_RuntimeHostCall_arg_i32(call, 1, &rhs) ||
        !fa_RuntimeHostCall_set_i32(call, 0, lhs + rhs)) {
        return FA_RUNTIME_ERR_TRAP;
    }

    return FA_RUNTIME_OK;
}

static int host_time_ms(fa_Runtime *runtime, const fa_RuntimeHostCall *call, void *user_data)
{
    struct timeval now;

    (void)runtime;
    (void)user_data;
    gettimeofday(&now, NULL);

    if (!fa_RuntimeHostCall_expect(call, 0, 1) ||
        !fa_RuntimeHostCall_set_i64(call, 0, ((i64)now.tv_sec * 1000) + (now.tv_usec / 1000))) {
        return FA_RUNTIME_ERR_TRAP;
    }

    return FA_RUNTIME_OK;
}

static int host_log_i32(fa_Runtime *runtime, const fa_RuntimeHostCall *call, void *user_data)
{
    i32 value = 0;

    (void)runtime;
    (void)user_data;

    if (!fa_RuntimeHostCall_expect(call, 1, 0) ||
        !fa_RuntimeHostCall_arg_i32(call, 0, &value)) {
        return FA_RUNTIME_ERR_TRAP;
    }

    ESP_LOGI(TAG, "wasm log: %ld", (long)value);
    return FA_RUNTIME_OK;
}

static esp_err_t bind_common_imports(watch_wasm_context_t *ctx)
{
    if (fa_Runtime_bindHostFunction(ctx->runtime, "env", "host_add", host_add, ctx) != FA_RUNTIME_OK ||
        fa_Runtime_bindHostFunction(ctx->runtime, "env", "host_time_ms", host_time_ms, ctx) != FA_RUNTIME_OK ||
        fa_Runtime_bindHostFunction(ctx->runtime, "env", "host_log_i32", host_log_i32, ctx) != FA_RUNTIME_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t load_module(watch_wasm_context_t *ctx)
{
    if (ctx->app->embedded_wasm != NULL && ctx->app->embedded_wasm_size > 0) {
        ctx->module = wasm_module_init_from_memory(ctx->app->embedded_wasm, ctx->app->embedded_wasm_size);
    } else if (ctx->app->storage_path != NULL) {
        ctx->module = wasm_module_init(ctx->app->storage_path);
    }

    if (ctx->module == NULL) {
        return ESP_FAIL;
    }

    if (wasm_load_header(ctx->module) != 0 ||
        wasm_scan_sections(ctx->module) != 0 ||
        wasm_load_types(ctx->module) != 0 ||
        wasm_load_functions(ctx->module) != 0 ||
        wasm_load_exports(ctx->module) != 0 ||
        wasm_load_tables(ctx->module) != 0 ||
        wasm_load_memories(ctx->module) != 0 ||
        wasm_load_globals(ctx->module) != 0 ||
        wasm_load_elements(ctx->module) != 0 ||
        wasm_load_data(ctx->module) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static int jit_spill(fa_Runtime *runtime,
                     uint32_t function_index,
                     const fa_JitProgram *program,
                     size_t program_bytes,
                     void *user_data)
{
    watch_wasm_context_t *ctx = user_data;
    char path[160];
    uint8_t *blob;
    size_t written = 0;
    const size_t needed = fa_jit_program_serialized_size(program);

    (void)runtime;
    (void)program_bytes;

    if ((ctx == NULL) || !watch_storage_available(ctx->storage) || (needed == 0)) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_make_blob_path(ctx->storage, ctx->app->id, "jit", function_index, path, sizeof(path)) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    blob = malloc(needed);
    if (blob == NULL) {
        return FA_RUNTIME_ERR_OUT_OF_MEMORY;
    }

    if (!fa_jit_program_serialize(program, blob, needed, &written) || (written != needed)) {
        free(blob);
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_write_blob(path, blob, written) != ESP_OK) {
        free(blob);
        return FA_RUNTIME_ERR_STREAM;
    }

    free(blob);
    return FA_RUNTIME_OK;
}

static int jit_load(fa_Runtime *runtime, uint32_t function_index, fa_JitProgram *program_out, void *user_data)
{
    watch_wasm_context_t *ctx = user_data;
    char path[160];
    uint8_t *blob = NULL;
    size_t size = 0;

    (void)runtime;

    if ((ctx == NULL) || !watch_storage_available(ctx->storage) || (program_out == NULL)) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_make_blob_path(ctx->storage, ctx->app->id, "jit", function_index, path, sizeof(path)) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_read_blob(path, &blob, &size) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (!fa_jit_program_deserialize(blob, size, program_out)) {
        free(blob);
        return FA_RUNTIME_ERR_TRAP;
    }

    free(blob);
    return FA_RUNTIME_OK;
}

static int memory_spill(fa_Runtime *runtime,
                        uint32_t memory_index,
                        const fa_RuntimeMemory *memory,
                        void *user_data)
{
    watch_wasm_context_t *ctx = user_data;
    char path[160];
    size_t written = 0;
    uint8_t *blob;
    const size_t needed = fa_Runtime_serializedMemorySize(runtime, memory_index);

    if ((ctx == NULL) || !watch_storage_available(ctx->storage) || (memory == NULL) || (needed == 0)) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_make_blob_path(ctx->storage, ctx->app->id, "mem", memory_index, path, sizeof(path)) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    blob = malloc(needed);
    if (blob == NULL) {
        return FA_RUNTIME_ERR_OUT_OF_MEMORY;
    }

    if (!fa_Runtime_serializeMemory(runtime, memory_index, blob, needed, &written) || (written != needed)) {
        free(blob);
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_write_blob(path, blob, written) != ESP_OK) {
        free(blob);
        return FA_RUNTIME_ERR_STREAM;
    }

    free(blob);
    return FA_RUNTIME_OK;
}

static int memory_load(fa_Runtime *runtime,
                       uint32_t memory_index,
                       fa_RuntimeMemory *memory,
                       void *user_data)
{
    watch_wasm_context_t *ctx = user_data;
    char path[160];
    uint8_t *blob = NULL;
    size_t size = 0;

    (void)memory;

    if ((ctx == NULL) || !watch_storage_available(ctx->storage)) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_make_blob_path(ctx->storage, ctx->app->id, "mem", memory_index, path, sizeof(path)) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (watch_storage_read_blob(path, &blob, &size) != ESP_OK) {
        return FA_RUNTIME_ERR_STREAM;
    }

    if (!fa_Runtime_deserializeMemory(runtime, memory_index, blob, size)) {
        free(blob);
        return FA_RUNTIME_ERR_STREAM;
    }

    free(blob);
    return FA_RUNTIME_OK;
}

static int function_trap(fa_Runtime *runtime, uint32_t function_index, void *user_data)
{
    watch_wasm_context_t *ctx = user_data;

    if ((ctx == NULL) || (function_index != ctx->entry_function_index)) {
        return FA_RUNTIME_OK;
    }

    if (ctx->runtime->memories_count > 0) {
        (void)fa_Runtime_loadMemory(runtime, 0);
    }
    (void)fa_Runtime_jitLoadProgram(runtime, function_index);
    (void)fa_Runtime_setFunctionTrap(runtime, function_index, false);
    ctx->hibernated = false;
    return FA_RUNTIME_OK;
}

static esp_err_t init_runtime_context(watch_wasm_context_t *ctx)
{
    fa_RuntimeSpillHooks spill_hooks;
    fa_RuntimeTrapHooks trap_hooks;

    if (load_module(ctx) != ESP_OK) {
        return ESP_FAIL;
    }

    ctx->runtime = fa_Runtime_init();
    if (ctx->runtime == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ctx->runtime->jit_context.config.min_ram_bytes = 0;
    ctx->runtime->jit_context.config.min_cpu_count = 1;
    ctx->runtime->jit_context.config.min_hot_loop_hits = 0;
    ctx->runtime->jit_context.config.min_executed_ops = 1;
    ctx->runtime->jit_context.config.min_advantage_score = 0.0f;
    ctx->runtime->jit_context.config.prescan_functions = true;

    if (fa_Runtime_attachModule(ctx->runtime, ctx->module) != FA_RUNTIME_OK) {
        return ESP_FAIL;
    }

    if (bind_common_imports(ctx) != ESP_OK) {
        return ESP_FAIL;
    }

    if (!find_exported_function(ctx->module, ctx->app->entry_export ? ctx->app->entry_export : "run", &ctx->entry_function_index)) {
        return ESP_ERR_NOT_FOUND;
    }

    ctx->job = fa_Runtime_createJob(ctx->runtime);
    if (ctx->job == NULL) {
        return ESP_ERR_NO_MEM;
    }

    spill_hooks = (fa_RuntimeSpillHooks){
        .jit_spill = jit_spill,
        .jit_load = jit_load,
        .memory_spill = memory_spill,
        .memory_load = memory_load,
        .user_data = ctx,
    };
    fa_Runtime_setSpillHooks(ctx->runtime, &spill_hooks);

    trap_hooks = (fa_RuntimeTrapHooks){
        .on_function_trap = function_trap,
        .user_data = ctx,
    };
    fa_Runtime_setTrapHooks(ctx->runtime, &trap_hooks);

    ctx->loaded = true;
    ctx->hibernated = false;
    ctx->last_status = FA_RUNTIME_OK;
    return ESP_OK;
}

static void free_context(watch_wasm_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if ((ctx->runtime != NULL) && (ctx->job != NULL)) {
        (void)fa_Runtime_destroyJob(ctx->runtime, ctx->job);
        ctx->job = NULL;
    }
    if (ctx->runtime != NULL) {
        fa_Runtime_free(ctx->runtime);
        ctx->runtime = NULL;
    }
    if (ctx->module != NULL) {
        wasm_module_free(ctx->module);
        ctx->module = NULL;
    }

    ctx->loaded = false;
    ctx->hibernated = false;
}

esp_err_t watch_wasm_runtime_init(watch_wasm_runtime_t *runtime, const watch_storage_t *storage)
{
    if ((runtime == NULL) || (storage == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->storage = storage;
    return ESP_OK;
}

esp_err_t watch_wasm_runtime_run(watch_wasm_runtime_t *runtime,
                                 const watch_os_app_descriptor_t *app,
                                 watch_wasm_result_t *result)
{
    watch_wasm_context_t *ctx;
    const fa_JobValue *value;

    if ((runtime == NULL) || (app == NULL) || (app->kind != WATCH_APP_KIND_WASM)) {
        return ESP_ERR_INVALID_ARG;
    }

    ctx = get_or_create_context(runtime, app);
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (!ctx->loaded) {
        esp_err_t init_err = init_runtime_context(ctx);
        if (init_err != ESP_OK) {
            free_context(ctx);
            reset_context_slot(runtime, ctx, app);
            return init_err;
        }
    }

    if (ctx->hibernated) {
        (void)watch_wasm_runtime_resume(runtime, app, NULL);
    }

    fa_JobStack_reset(&ctx->job->stack);
    ctx->last_status = fa_Runtime_executeJob(ctx->runtime, ctx->job, ctx->entry_function_index);
    if (ctx->last_status != FA_RUNTIME_OK) {
        ESP_LOGW(TAG, "wasm app %s failed: %s", app->id, fayasm_port_status_string(ctx->last_status));
        publish_result(ctx, result);
        return ESP_FAIL;
    }

    value = fa_JobStack_peek(&ctx->job->stack, 0);
    if ((value != NULL) && (value->kind == fa_job_value_i32)) {
        ctx->has_last_result = true;
        ctx->last_result_i32 = value->payload.i32_value;
    }

    publish_result(ctx, result);
    return ESP_OK;
}

esp_err_t watch_wasm_runtime_hibernate(watch_wasm_runtime_t *runtime,
                                       const watch_os_app_descriptor_t *app,
                                       watch_wasm_result_t *result)
{
    watch_wasm_context_t *ctx = find_context(runtime, app);

    if (ctx == NULL || !ctx->loaded) {
        return ESP_OK;
    }

    if (!watch_storage_available(ctx->storage)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->runtime->memories_count > 0) {
        (void)fa_Runtime_spillMemory(ctx->runtime, 0);
    }
    (void)fa_Runtime_jitSpillProgram(ctx->runtime, ctx->entry_function_index);
    (void)fa_Runtime_setFunctionTrap(ctx->runtime, ctx->entry_function_index, true);
    ctx->hibernated = true;
    publish_result(ctx, result);
    return ESP_OK;
}

esp_err_t watch_wasm_runtime_resume(watch_wasm_runtime_t *runtime,
                                    const watch_os_app_descriptor_t *app,
                                    watch_wasm_result_t *result)
{
    watch_wasm_context_t *ctx = find_context(runtime, app);

    if (ctx == NULL || !ctx->loaded) {
        return ESP_ERR_NOT_FOUND;
    }

    if (ctx->runtime->memories_count > 0) {
        (void)fa_Runtime_loadMemory(ctx->runtime, 0);
    }
    (void)fa_Runtime_jitLoadProgram(ctx->runtime, ctx->entry_function_index);
    (void)fa_Runtime_setFunctionTrap(ctx->runtime, ctx->entry_function_index, false);
    ctx->hibernated = false;
    publish_result(ctx, result);
    return ESP_OK;
}

esp_err_t watch_wasm_runtime_unload(watch_wasm_runtime_t *runtime, const watch_os_app_descriptor_t *app)
{
    watch_wasm_context_t *ctx = find_context(runtime, app);

    if (ctx == NULL) {
        return ESP_OK;
    }

    free_context(ctx);
    memset(ctx, 0, sizeof(*ctx));
    return ESP_OK;
}
