#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smart_os_process.h"

#define SMART_OS_PROCESS_MANAGER_MAX_APPS 8

typedef struct {
    smart_os_process_snapshot_t slots[SMART_OS_PROCESS_MANAGER_MAX_APPS];
    size_t count;
    uint32_t internal_ram_floor_bytes;
    uint32_t psram_floor_bytes;
} smart_os_process_manager_t;

void smart_os_process_manager_init(smart_os_process_manager_t *manager,
                                uint32_t internal_ram_floor_bytes,
                                uint32_t psram_floor_bytes);
void smart_os_process_manager_register_catalog(smart_os_process_manager_t *manager,
                                            const smart_os_app_descriptor_t *catalog,
                                            size_t count);
smart_os_process_snapshot_t *smart_os_process_manager_find(smart_os_process_manager_t *manager, const char *app_id);
const smart_os_process_snapshot_t *smart_os_process_manager_at(const smart_os_process_manager_t *manager, size_t index);
size_t smart_os_process_manager_count(const smart_os_process_manager_t *manager);
void smart_os_process_manager_mark_state(smart_os_process_snapshot_t *process,
                                      smart_os_process_state_t state,
                                      bool resident_in_ram);
void smart_os_process_manager_set_result(smart_os_process_snapshot_t *process, int status, bool has_result, int32_t value);
bool smart_os_process_manager_memory_pressure(const smart_os_process_manager_t *manager,
                                           size_t *internal_free_bytes,
                                           size_t *psram_free_bytes);
smart_os_process_snapshot_t *smart_os_process_manager_pick_victim(smart_os_process_manager_t *manager);
