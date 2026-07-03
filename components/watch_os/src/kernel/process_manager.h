#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "watch_os_process.h"

#define WATCH_PROCESS_MANAGER_MAX_APPS 8

typedef struct {
    watch_os_process_snapshot_t slots[WATCH_PROCESS_MANAGER_MAX_APPS];
    size_t count;
    uint32_t internal_ram_floor_bytes;
    uint32_t psram_floor_bytes;
} watch_process_manager_t;

void watch_process_manager_init(watch_process_manager_t *manager,
                                uint32_t internal_ram_floor_bytes,
                                uint32_t psram_floor_bytes);
void watch_process_manager_register_catalog(watch_process_manager_t *manager,
                                            const watch_os_app_descriptor_t *catalog,
                                            size_t count);
watch_os_process_snapshot_t *watch_process_manager_find(watch_process_manager_t *manager, const char *app_id);
const watch_os_process_snapshot_t *watch_process_manager_at(const watch_process_manager_t *manager, size_t index);
size_t watch_process_manager_count(const watch_process_manager_t *manager);
void watch_process_manager_mark_state(watch_os_process_snapshot_t *process,
                                      watch_process_state_t state,
                                      bool resident_in_ram);
void watch_process_manager_set_result(watch_os_process_snapshot_t *process, int status, bool has_result, int32_t value);
bool watch_process_manager_memory_pressure(const watch_process_manager_t *manager,
                                           size_t *internal_free_bytes,
                                           size_t *psram_free_bytes);
watch_os_process_snapshot_t *watch_process_manager_pick_victim(watch_process_manager_t *manager);
