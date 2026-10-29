#include "serial_uploader.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "native_ui.h"
#include "esp_check.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

// Snapshot rendering traverses the LVGL tree synchronously on this task.
#define WATCH_SERIAL_UPLOAD_TASK_STACK (12 * 1024)
#define WATCH_SERIAL_UPLOAD_TASK_PRIO 3
#define WATCH_SERIAL_UPLOAD_LINE_MAX 384
#define WATCH_SERIAL_UPLOAD_CHUNK_MAX 96

typedef struct {
    const watch_storage_t *storage;
    FILE *file;
    size_t expected_size;
    size_t received_size;
    char target_path[160];
    char temp_path[168];
    char app_slug[48];
} watch_serial_upload_session_t;

static const char *TAG = "watch_serial";
static TaskHandle_t s_task;
static watch_serial_upload_session_t s_session;

static void protocol_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static void clear_session(bool remove_temp)
{
    const watch_storage_t *storage = s_session.storage;

    if (s_session.file != NULL) {
        fclose(s_session.file);
        s_session.file = NULL;
    }

    if (remove_temp && s_session.temp_path[0] != '\0') {
        unlink(s_session.temp_path);
    }

    memset(&s_session, 0, sizeof(s_session));
    s_session.storage = storage;
}

static bool sanitize_slug(const char *input, char *output, size_t output_size)
{
    size_t cursor = 0;

    if ((input == NULL) || (output == NULL) || (output_size < 2)) {
        return false;
    }

    for (size_t i = 0; input[i] != '\0' && cursor + 1 < output_size; ++i) {
        const unsigned char ch = (unsigned char)input[i];

        if (isalnum(ch)) {
            output[cursor++] = (char)tolower(ch);
        } else if (ch == '-' || ch == '_') {
            output[cursor++] = (char)ch;
        } else {
            output[cursor++] = '_';
        }
    }

    output[cursor] = '\0';
    return cursor > 0;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

static esp_err_t begin_upload(const char *raw_slug, size_t size_bytes)
{
    char safe_slug[sizeof(s_session.app_slug)];
    int written;

    if (s_session.file != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!watch_storage_available(s_session.storage)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!sanitize_slug(raw_slug, safe_slug, sizeof(safe_slug))) {
        return ESP_ERR_INVALID_ARG;
    }

    if (watch_storage_make_app_wasm_path(s_session.storage, safe_slug, s_session.target_path, sizeof(s_session.target_path)) != ESP_OK) {
        return ESP_FAIL;
    }

    if (watch_storage_prepare_external_wasm_path(s_session.storage, s_session.target_path) != ESP_OK) {
        return ESP_FAIL;
    }

    written = snprintf(s_session.temp_path, sizeof(s_session.temp_path), "%s.part", s_session.target_path);
    if (written <= 0 || (size_t)written >= sizeof(s_session.temp_path)) {
        return ESP_ERR_INVALID_SIZE;
    }

    unlink(s_session.temp_path);
    s_session.file = fopen(s_session.temp_path, "wb");
    if (s_session.file == NULL) {
        return ESP_FAIL;
    }

    s_session.expected_size = size_bytes;
    s_session.received_size = 0;
    snprintf(s_session.app_slug, sizeof(s_session.app_slug), "%s", safe_slug);
    return ESP_OK;
}

static esp_err_t write_chunk(const char *hex)
{
    uint8_t decoded[WATCH_SERIAL_UPLOAD_CHUNK_MAX];
    size_t length = 0;

    if (s_session.file == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (hex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (hex[length] != '\0') {
        if (hex[length] == '\r' || hex[length] == '\n') {
            break;
        }
        ++length;
    }

    if ((length == 0) || ((length % 2) != 0) || ((length / 2) > sizeof(decoded))) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < length; i += 2) {
        const int hi = hex_value(hex[i]);
        const int lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        decoded[i / 2] = (uint8_t)((hi << 4) | lo);
    }

    if ((s_session.received_size + (length / 2)) > s_session.expected_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (fwrite(decoded, 1, length / 2, s_session.file) != (length / 2)) {
        return ESP_FAIL;
    }

    s_session.received_size += (length / 2);
    return ESP_OK;
}

static esp_err_t finish_upload(void)
{
    if (s_session.file == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_session.received_size != s_session.expected_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    fclose(s_session.file);
    s_session.file = NULL;
    unlink(s_session.target_path);
    if (rename(s_session.temp_path, s_session.target_path) != 0) {
        unlink(s_session.temp_path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "installed %s (%u bytes) at %s",
             s_session.app_slug,
             (unsigned)s_session.received_size,
             s_session.target_path);
    return ESP_OK;
}

static void handle_line(char *line)
{
    char slug[48];
    unsigned long size_bytes = 0;
    esp_err_t err;

    if (line == NULL) {
        return;
    }

    if (strncmp(line, "WATCH-UI ", 9) == 0) {
        int x, y;
        if (strcmp(line, "WATCH-UI BUTTON\n") == 0) {
            watch_native_ui_request(WATCH_UI_ACTION_BUTTON);
            protocol_printf("WATCH-UI OK button");
        } else if (sscanf(line, "WATCH-UI TAP %d %d", &x, &y) == 2) {
            err = watch_native_ui_inject_tap(x, y);
            protocol_printf("WATCH-UI %s tap", err == ESP_OK ? "OK" : "ERROR");
        } else if (strcmp(line, "WATCH-UI SNAPSHOT\n") == 0) {
            err = watch_native_ui_snapshot();
            if (err != ESP_OK) {
                protocol_printf("WATCH-UI ERROR snapshot %s", esp_err_to_name(err));
            }
        } else {
            protocol_printf("WATCH-UI ERROR command");
        }
        return;
    }

    if (strncmp(line, "WASM-UPLOAD ", 12) != 0) {
        return;
    }

    if (strcmp(line, "WASM-UPLOAD PING\n") == 0 || strcmp(line, "WASM-UPLOAD PING\r\n") == 0) {
        protocol_printf("WASM-UPLOAD PONG");
        return;
    }

    if (strcmp(line, "WASM-UPLOAD HELP\n") == 0 || strcmp(line, "WASM-UPLOAD HELP\r\n") == 0) {
        protocol_printf("WASM-UPLOAD INFO BEGIN <app> <bytes> | DATA <hex> | END | ABORT");
        return;
    }

    if (strcmp(line, "WASM-UPLOAD ABORT\n") == 0 || strcmp(line, "WASM-UPLOAD ABORT\r\n") == 0) {
        clear_session(true);
        protocol_printf("WASM-UPLOAD ABORTED");
        return;
    }

    if (sscanf(line, "WASM-UPLOAD BEGIN %47s %lu", slug, &size_bytes) == 2) {
        err = begin_upload(slug, (size_t)size_bytes);
        if (err == ESP_OK) {
            protocol_printf("WASM-UPLOAD READY %s %s", s_session.app_slug, s_session.target_path);
        } else {
            clear_session(true);
            protocol_printf("WASM-UPLOAD ERROR begin %s", esp_err_to_name(err));
        }
        return;
    }

    if (strncmp(line, "WASM-UPLOAD DATA ", 17) == 0) {
        err = write_chunk(line + 17);
        if (err == ESP_OK) {
            protocol_printf("WASM-UPLOAD ACK %u", (unsigned)s_session.received_size);
        } else {
            clear_session(true);
            protocol_printf("WASM-UPLOAD ERROR data %s", esp_err_to_name(err));
        }
        return;
    }

    if (strcmp(line, "WASM-UPLOAD END\n") == 0 || strcmp(line, "WASM-UPLOAD END\r\n") == 0) {
        err = finish_upload();
        if (err == ESP_OK) {
            protocol_printf("WASM-UPLOAD OK %s %u", s_session.target_path, (unsigned)s_session.received_size);
            clear_session(false);
        } else {
            clear_session(true);
            protocol_printf("WASM-UPLOAD ERROR end %s", esp_err_to_name(err));
        }
        return;
    }

    protocol_printf("WASM-UPLOAD ERROR command ESP_ERR_INVALID_ARG");
}

static void serial_uploader_task(void *arg)
{
    char line[WATCH_SERIAL_UPLOAD_LINE_MAX];
    size_t used = 0;
    bool overflow = false;

    s_session.storage = arg;
    setvbuf(stdin, NULL, _IONBF, 0);
    protocol_printf("WASM-UPLOAD INFO ready apps_root=%s", watch_storage_apps_root(s_session.storage));

    while (true) {
        int ch = fgetc(stdin);
        if (ch == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (overflow) {
                protocol_printf("WASM-UPLOAD ERROR line-too-long");
            } else {
                line[used++] = '\n';
                line[used] = '\0';
                handle_line(line);
            }
            used = 0;
            overflow = false;
        } else if (used + 2 < sizeof(line)) {
            line[used++] = (char)ch;
        } else {
            overflow = true;
        }
    }
}

esp_err_t watch_serial_uploader_start(const watch_storage_t *storage)
{
    if (storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!watch_storage_available(storage)) {
        ESP_LOGW(TAG, "WASM install unavailable without microSD; UI diagnostics remain enabled");
    }

    if (s_task != NULL) {
        return ESP_OK;
    }

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = {
            .rx_buffer_size = 1024,
            .tx_buffer_size = 2048,
        };
        ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&config), TAG, "USB serial driver install failed");
    }
    usb_serial_jtag_vfs_use_driver();
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
#endif

    clear_session(false);
    s_session.storage = storage;

    if (xTaskCreatePinnedToCore(serial_uploader_task,
                                "watch_serial_upload",
                                WATCH_SERIAL_UPLOAD_TASK_STACK,
                                (void *)storage,
                                WATCH_SERIAL_UPLOAD_TASK_PRIO,
                                &s_task,
                                tskNO_AFFINITY) != pdPASS) {
        s_task = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "serial uploader enabled for %s", watch_storage_apps_root(storage));
    return ESP_OK;
}
