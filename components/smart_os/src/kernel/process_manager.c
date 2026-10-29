#include "process_manager.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"

const char *smart_os_process_state_label(smart_os_process_state_t state)
{
    switch (state) {
    case SMART_OS_PROCESS_STOPPED:
        return "stopped";
    case SMART_OS_PROCESS_READY:
        return "ready";
    case SMART_OS_PROCESS_RUNNING:
        return "running";
    case SMART_OS_PROCESS_HIBERNATED:
        return "hibernated";
    case SMART_OS_PROCESS_FAULTED:
        return "faulted";
    default:
        return "unknown";
    }
}

const char *smart_os_residency_label(smart_os_app_residency_t residency)
{
    switch (residency) {
    case SMART_OS_APP_RESIDENCY_ALWAYS_RESIDENT:
        return "always";
    case SMART_OS_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE:
        return "active";
    case SMART_OS_APP_RESIDENCY_HIBERNABLE:
        return "hibernate";
    case SMART_OS_APP_RESIDENCY_DISPOSABLE:
        return "dispose";
    default:
        return "unknown";
    }
}

void smart_os_process_manager_init(smart_os_process_manager_t *manager,
                                uint32_t internal_ram_floor_bytes,
                                uint32_t psram_floor_bytes)
{
    memset(manager, 0, sizeof(*manager));
    manager->internal_ram_floor_bytes = internal_ram_floor_bytes;
    manager->psram_floor_bytes = psram_floor_bytes;
}

void smart_os_process_manager_register_catalog(smart_os_process_manager_t *manager,
                                            const smart_os_app_descriptor_t *catalog,
                                            size_t count)
{
    const size_t limit = (count < SMART_OS_PROCESS_MANAGER_MAX_APPS) ? count : SMART_OS_PROCESS_MANAGER_MAX_APPS;

    manager->count = limit;
    for (size_t i = 0; i < limit; ++i) {
        manager->slots[i].app = &catalog[i];
        manager->slots[i].state = catalog[i].autostart ? SMART_OS_PROCESS_READY : SMART_OS_PROCESS_STOPPED;
        manager->slots[i].resident_in_ram = (catalog[i].residency == SMART_OS_APP_RESIDENCY_ALWAYS_RESIDENT);
        manager->slots[i].last_runtime_status = 0;
        manager->slots[i].last_state_change_ms = (uint64_t)(esp_timer_get_time() / 1000);
    }
}

smart_os_process_snapshot_t *smart_os_process_manager_find(smart_os_process_manager_t *manager, const char *app_id)
{
    if ((manager == NULL) || (app_id == NULL)) {
        return NULL;
    }

    for (size_t i = 0; i < manager->count; ++i) {
        if (manager->slots[i].app != NULL &&
            manager->slots[i].app->id != NULL &&
            strcmp(manager->slots[i].app->id, app_id) == 0) {
            return &manager->slots[i];
        }
    }

    return NULL;
}

const smart_os_process_snapshot_t *smart_os_process_manager_at(const smart_os_process_manager_t *manager, size_t index)
{
    if ((manager == NULL) || (index >= manager->count)) {
        return NULL;
    }

    return &manager->slots[index];
}

size_t smart_os_process_manager_count(const smart_os_process_manager_t *manager)
{
    return (manager != NULL) ? manager->count : 0;
}

void smart_os_process_manager_mark_state(smart_os_process_snapshot_t *process,
                                      smart_os_process_state_t state,
                                      bool resident_in_ram)
{
    if (process == NULL) {
        return;
    }

    process->state = state;
    process->resident_in_ram = resident_in_ram;
    process->last_state_change_ms = (uint64_t)(esp_timer_get_time() / 1000);
}

void smart_os_process_manager_set_result(smart_os_process_snapshot_t *process, int status, bool has_result, int32_t value)
{
    if (process == NULL) {
        return;
    }

    process->last_runtime_status = status;
    process->has_last_result = has_result;
    process->last_result_i32 = value;
}

bool smart_os_process_manager_memory_pressure(const smart_os_process_manager_t *manager,
                                           size_t *internal_free_bytes,
                                           size_t *psram_free_bytes)
{
    const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    if (internal_free_bytes != NULL) {
        *internal_free_bytes = internal_free;
    }
    if (psram_free_bytes != NULL) {
        *psram_free_bytes = psram_free;
    }

    if (manager == NULL) {
        return false;
    }

    return (internal_free < manager->internal_ram_floor_bytes) ||
           (psram_free < manager->psram_floor_bytes);
}

smart_os_process_snapshot_t *smart_os_process_manager_pick_victim(smart_os_process_manager_t *manager)
{
    smart_os_process_snapshot_t *best = NULL;

    if (manager == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < manager->count; ++i) {
        smart_os_process_snapshot_t *candidate = &manager->slots[i];

        if ((candidate->app == NULL) ||
            (candidate->app->kind != SMART_OS_APP_KIND_WASM) ||
            candidate->foreground ||
            !candidate->resident_in_ram) {
            continue;
        }

        if (candidate->app->residency == SMART_OS_APP_RESIDENCY_ALWAYS_RESIDENT ||
            candidate->app->residency == SMART_OS_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE) {
            continue;
        }

        if ((best == NULL) ||
            (candidate->app->residency > best->app->residency) ||
            (candidate->last_state_change_ms < best->last_state_change_ms)) {
            best = candidate;
        }
    }

    return best;
}
