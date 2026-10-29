#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WATCH_APP_KIND_NATIVE = 0,
    WATCH_APP_KIND_WASM = 1,
} watch_app_kind_t;

typedef enum {
    // Residency governs background eviction. The foreground app is protected
    // by the process manager regardless of class; disposable Settings is
    // explicitly unloaded by the OS when navigation leaves the app.
    WATCH_APP_RESIDENCY_ALWAYS_RESIDENT = 0,
    WATCH_APP_RESIDENCY_RESIDENT_WHILE_ACTIVE = 1,
    WATCH_APP_RESIDENCY_HIBERNABLE = 2,
    WATCH_APP_RESIDENCY_DISPOSABLE = 3,
} watch_app_residency_t;

typedef struct {
    const char *id;
    const char *title;
    watch_app_kind_t kind;
    watch_app_residency_t residency;
    uint32_t ram_budget_bytes;
    bool autostart;
    bool native_critical_ui;
    const char *entry_export;
    const char *storage_path;
    const uint8_t *embedded_wasm;
    size_t embedded_wasm_size;
} watch_os_app_descriptor_t;
