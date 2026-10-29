#pragma once

#include "esp_err.h"
#include "storage.h"

esp_err_t smart_os_serial_uploader_start(const smart_os_storage_t *storage);
