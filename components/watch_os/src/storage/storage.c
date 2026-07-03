#include "storage.h"

#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"

static const char *TAG = "watch_storage";

static void sanitize_id(const char *input, char *output, size_t output_size)
{
    size_t cursor = 0;

    if ((output == NULL) || (output_size == 0)) {
        return;
    }

    if (input == NULL) {
        output[0] = '\0';
        return;
    }

    for (size_t i = 0; input[i] != '\0' && cursor + 1 < output_size; ++i) {
        const char ch = input[i];
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            output[cursor++] = ch;
        } else {
            output[cursor++] = '_';
        }
    }

    output[cursor] = '\0';
}

static esp_err_t ensure_directory(const char *path)
{
    struct stat st = {0};

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_FAIL;
    }

    if (mkdir(path, 0775) == 0) {
        return ESP_OK;
    }

    if (errno == EEXIST) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

static esp_err_t ensure_parent_directories(const char *path)
{
    char partial[160];
    size_t length;

    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    length = strnlen(path, sizeof(partial));
    if (length == 0 || length >= sizeof(partial)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(partial, path, length + 1);
    for (size_t i = 1; i < length; ++i) {
        if (partial[i] != '/') {
            continue;
        }

        partial[i] = '\0';
        if (ensure_directory(partial) != ESP_OK) {
            return ESP_FAIL;
        }
        partial[i] = '/';
    }

    return ESP_OK;
}

esp_err_t watch_storage_init(watch_storage_t *storage, bool prefer_sdcard_runtime)
{
    esp_err_t mount_err;
    if (storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(storage, 0, sizeof(*storage));
    snprintf(storage->root, sizeof(storage->root), "/sdcard/smartesp");
    snprintf(storage->runtime_dir, sizeof(storage->runtime_dir), "%s/runtime", storage->root);
    snprintf(storage->apps_dir, sizeof(storage->apps_dir), "/sdcard/apps");

    if (!prefer_sdcard_runtime) {
        ESP_LOGI(TAG, "sd-backed runtime disabled by config");
        return ESP_OK;
    }

    mount_err = bsp_sdcard_mount();
    if (mount_err != ESP_OK && mount_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG,
                 "microSD mount failed err=%s; insert a card formatted as FAT32 to enable runtime spill and external WASM loading",
                 esp_err_to_name(mount_err));
        return ESP_OK;
    }

    struct stat sd_root = {0};
    if (stat("/sdcard", &sd_root) != 0 || !S_ISDIR(sd_root.st_mode)) {
        ESP_LOGW(TAG, "/sdcard mount point is unavailable, hibernation and serial WASM install stay disabled");
        return ESP_OK;
    }

    if (ensure_directory(storage->root) != ESP_OK ||
        ensure_directory(storage->runtime_dir) != ESP_OK ||
        ensure_directory(storage->apps_dir) != ESP_OK) {
        ESP_LOGW(TAG, "runtime directories could not be prepared");
        return ESP_FAIL;
    }

    storage->mounted = true;
    ESP_LOGI(TAG, "runtime storage ready at %s (apps under %s)", storage->runtime_dir, storage->apps_dir);
    return ESP_OK;
}

bool watch_storage_available(const watch_storage_t *storage)
{
    return (storage != NULL) && storage->mounted;
}

const char *watch_storage_root(const watch_storage_t *storage)
{
    return (storage != NULL) ? storage->runtime_dir : "";
}

const char *watch_storage_apps_root(const watch_storage_t *storage)
{
    return (storage != NULL) ? storage->apps_dir : "";
}

bool watch_storage_file_exists(const char *path)
{
    struct stat st = {0};

    return (path != NULL) && (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

esp_err_t watch_storage_make_app_wasm_path(const watch_storage_t *storage,
                                           const char *app_slug,
                                           char *out,
                                           size_t out_size)
{
    char safe_id[48];

    if ((storage == NULL) || (app_slug == NULL) || (out == NULL) || (out_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_storage_available(storage)) {
        return ESP_ERR_INVALID_STATE;
    }

    sanitize_id(app_slug, safe_id, sizeof(safe_id));
    snprintf(out, out_size, "%s/%s/app.wasm", storage->apps_dir, safe_id);
    return ESP_OK;
}

esp_err_t watch_storage_prepare_external_wasm_path(const watch_storage_t *storage, const char *path)
{
    if ((storage == NULL) || (path == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_storage_available(storage)) {
        return ESP_ERR_INVALID_STATE;
    }

    return ensure_parent_directories(path);
}

esp_err_t watch_storage_make_blob_path(const watch_storage_t *storage,
                                       const char *app_id,
                                       const char *kind,
                                       uint32_t index,
                                       char *out,
                                       size_t out_size)
{
    char safe_id[48];

    if ((storage == NULL) || (app_id == NULL) || (kind == NULL) || (out == NULL) || (out_size == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_storage_available(storage)) {
        return ESP_ERR_INVALID_STATE;
    }

    sanitize_id(app_id, safe_id, sizeof(safe_id));
    snprintf(out, out_size, "%s/%s_%s_%" PRIu32 ".bin", storage->runtime_dir, safe_id, kind, index);
    return ESP_OK;
}

esp_err_t watch_storage_write_blob(const char *path, const uint8_t *blob, size_t size)
{
    FILE *file;

    if ((path == NULL) || ((blob == NULL) && (size > 0))) {
        return ESP_ERR_INVALID_ARG;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return ESP_FAIL;
    }

    if ((size > 0) && (fwrite(blob, 1, size, file) != size)) {
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);
    return ESP_OK;
}

esp_err_t watch_storage_read_blob(const char *path, uint8_t **blob_out, size_t *size_out)
{
    FILE *file;
    uint8_t *blob = NULL;
    size_t size = 0;

    if ((path == NULL) || (blob_out == NULL) || (size_out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    *blob_out = NULL;
    *size_out = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ESP_FAIL;
    }

    const long end = ftell(file);
    if ((end < 0) || (fseek(file, 0, SEEK_SET) != 0)) {
        fclose(file);
        return ESP_FAIL;
    }

    size = (size_t)end;
    blob = malloc(size > 0 ? size : 1);
    if (blob == NULL) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    if ((size > 0) && (fread(blob, 1, size, file) != size)) {
        free(blob);
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);

    *blob_out = blob;
    *size_out = size;
    return ESP_OK;
}
