#include <stdint.h>

#if defined(__clang__)
#define WATCH_WASM_IMPORT(module_name, import_symbol) \
    __attribute__((import_module(module_name), import_name(import_symbol)))
#else
#define WATCH_WASM_IMPORT(module_name, import_name)
#endif

extern int64_t host_time_ms(void) WATCH_WASM_IMPORT("env", "host_time_ms");
extern void host_log_i32(int32_t value) WATCH_WASM_IMPORT("env", "host_log_i32");

static int32_t encode_hhmmss(int64_t total_seconds)
{
    const int32_t hours = (int32_t)((total_seconds / 3600) % 24);
    const int32_t minutes = (int32_t)((total_seconds / 60) % 60);
    const int32_t seconds = (int32_t)(total_seconds % 60);
    return (hours * 10000) + (minutes * 100) + seconds;
}

int run(void)
{
    const int32_t encoded = encode_hhmmss(host_time_ms() / 1000);
    host_log_i32(encoded);
    return encoded;
}
