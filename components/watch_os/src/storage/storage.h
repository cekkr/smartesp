#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool mounted;
    char root[64];
    char runtime_dir[96];
} watch_storage_t;

esp_err_t watch_storage_init(watch_storage_t *storage, bool prefer_sdcard_runtime);
bool watch_storage_available(const watch_storage_t *storage);
const char *watch_storage_root(const watch_storage_t *storage);

esp_err_t watch_storage_make_blob_path(const watch_storage_t *storage,
                                       const char *app_id,
                                       const char *kind,
                                       uint32_t index,
                                       char *out,
                                       size_t out_size);
esp_err_t watch_storage_write_blob(const char *path, const uint8_t *blob, size_t size);
esp_err_t watch_storage_read_blob(const char *path, uint8_t **blob_out, size_t *size_out);
