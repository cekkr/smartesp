#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "smart_os_app.h"

typedef enum {
    SMART_OS_PROCESS_STOPPED = 0,
    SMART_OS_PROCESS_READY = 1,
    SMART_OS_PROCESS_RUNNING = 2,
    SMART_OS_PROCESS_HIBERNATED = 3,
    SMART_OS_PROCESS_FAULTED = 4,
} smart_os_process_state_t;

typedef struct {
    const smart_os_app_descriptor_t *app;
    smart_os_process_state_t state;
    bool resident_in_ram;
    bool foreground; // Active app is protected from memory-pressure eviction.
    bool has_last_result;
    int32_t last_result_i32;
    int last_runtime_status;
    uint64_t last_state_change_ms;
} smart_os_process_snapshot_t;

const char *smart_os_process_state_label(smart_os_process_state_t state);
const char *smart_os_residency_label(smart_os_app_residency_t residency);
