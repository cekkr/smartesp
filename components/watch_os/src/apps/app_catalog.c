#include "app_catalog.h"

#include "clock_demo_wasm.h"
#include "sample_add_wasm.h"

static const watch_os_app_descriptor_t k_catalog[] = {
    {
        .id = "sys.ui",
        .title = "Shell",
        .kind = WATCH_APP_KIND_NATIVE,
        .residency = WATCH_APP_RESIDENCY_ALWAYS_RESIDENT,
        .ram_budget_bytes = 64 * 1024,
        .autostart = true,
        .native_critical_ui = true,
    },
    {
        .id = "sys.input",
        .title = "Input",
        .kind = WATCH_APP_KIND_NATIVE,
        .residency = WATCH_APP_RESIDENCY_ALWAYS_RESIDENT,
        .ram_budget_bytes = 32 * 1024,
        .autostart = true,
        .native_critical_ui = true,
    },
    {
        .id = "sys.notifications",
        .title = "Notifications",
        .kind = WATCH_APP_KIND_NATIVE,
        .residency = WATCH_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE,
        .ram_budget_bytes = 48 * 1024,
        .autostart = true,
        .native_critical_ui = false,
    },
    {
        .id = "wasm.demo.add",
        .title = "Demo Add",
        .kind = WATCH_APP_KIND_WASM,
        .residency = WATCH_APP_RESIDENCY_HIBERNABLE,
        .ram_budget_bytes = 96 * 1024,
        .autostart = false,
        .native_critical_ui = false,
        .entry_export = "run",
        .embedded_wasm = k_watch_demo_add_wasm,
        .embedded_wasm_size = sizeof(k_watch_demo_add_wasm),
    },
    {
        .id = "wasm.demo.clock",
        .title = "Demo Clock",
        .kind = WATCH_APP_KIND_WASM,
        .residency = WATCH_APP_RESIDENCY_HIBERNABLE,
        .ram_budget_bytes = 96 * 1024,
        .autostart = false,
        .native_critical_ui = false,
        .entry_export = "run",
        .embedded_wasm = k_watch_demo_clock_wasm,
        .embedded_wasm_size = sizeof(k_watch_demo_clock_wasm),
    },
    {
        .id = "wasm.workout.stub",
        .title = "Workout",
        .kind = WATCH_APP_KIND_WASM,
        .residency = WATCH_APP_RESIDENCY_DISPOSABLE,
        .ram_budget_bytes = 160 * 1024,
        .autostart = false,
        .native_critical_ui = false,
        .entry_export = "run",
        .storage_path = "/sdcard/apps/workout/app.wasm",
    },
};

const watch_os_app_descriptor_t *watch_app_catalog_get(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = sizeof(k_catalog) / sizeof(k_catalog[0]);
    }

    return k_catalog;
}
