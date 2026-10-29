#include "esp_check.h"
#include "nvs_flash.h"
#include "smart_os.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    const smart_os_config_t config = SMART_OS_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(smart_os_start(&config));
}
