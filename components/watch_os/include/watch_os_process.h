#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "watch_os_app.h"

typedef enum {
    WATCH_PROCESS_STOPPED = 0,
    WATCH_PROCESS_READY = 1,
    WATCH_PROCESS_RUNNING = 2,
    WATCH_PROCESS_HIBERNATED = 3,
    WATCH_PROCESS_FAULTED = 4,
} watch_process_state_t;

typedef struct {
    const watch_os_app_descriptor_t *app;
    watch_process_state_t state;
    bool resident_in_ram;
    bool foreground; // Active app is protected from memory-pressure eviction.
    bool has_last_result;
    int32_t last_result_i32;
    int last_runtime_status;
    uint64_t last_state_change_ms;
} watch_os_process_snapshot_t;

const char *watch_process_state_label(watch_process_state_t state);
const char *watch_residency_label(watch_app_residency_t residency);
