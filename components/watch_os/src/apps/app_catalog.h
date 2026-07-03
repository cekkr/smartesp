#pragma once

#include <stddef.h>

#include "watch_os_app.h"

const watch_os_app_descriptor_t *watch_app_catalog_get(size_t *count_out);
