#pragma once

#include "esp_err.h"
#include "storage.h"

esp_err_t watch_serial_uploader_start(const watch_storage_t *storage);
