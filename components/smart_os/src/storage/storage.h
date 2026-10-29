#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool mounted;
    char root[64];
    char runtime_dir[96];
    char apps_dir[64];
} smart_os_storage_t;

esp_err_t smart_os_storage_init(smart_os_storage_t *storage, bool prefer_sdcard_runtime);
bool smart_os_storage_available(const smart_os_storage_t *storage);
const char *smart_os_storage_root(const smart_os_storage_t *storage);
const char *smart_os_storage_apps_root(const smart_os_storage_t *storage);
bool smart_os_storage_file_exists(const char *path);
esp_err_t smart_os_storage_make_app_wasm_path(const smart_os_storage_t *storage,
                                           const char *app_slug,
                                           char *out,
                                           size_t out_size);
esp_err_t smart_os_storage_prepare_external_wasm_path(const smart_os_storage_t *storage, const char *path);

esp_err_t smart_os_storage_make_blob_path(const smart_os_storage_t *storage,
                                       const char *app_id,
                                       const char *kind,
                                       uint32_t index,
                                       char *out,
                                       size_t out_size);
esp_err_t smart_os_storage_write_blob(const char *path, const uint8_t *blob, size_t size);
esp_err_t smart_os_storage_read_blob(const char *path, uint8_t **blob_out, size_t *size_out);
