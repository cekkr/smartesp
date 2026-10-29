#include <stdint.h>

extern int32_t host_app_action(void) __attribute__((import_module("env"), import_name("host_app_action")));
extern int32_t host_app_value(void) __attribute__((import_module("env"), import_name("host_app_value")));

// The OS owns the display and persistence. This app owns brightness policy.
int run(void)
{
    int32_t value = host_app_value();
    const int32_t action = host_app_action();
    if (action == -1) {
        value -= 20;
    } else if (action == 1) {
        value += 20;
    }
    if (value < 20) {
        value = 20;
    }
    if (value > 100) {
        value = 100;
    }
    return value;
}
