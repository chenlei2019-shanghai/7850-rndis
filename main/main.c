#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "usb_rndis_host.h"
#include "net_bridge.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "%s v%s", PROJECT_NAME, FIRMWARE_VERSION);
    ESP_LOGI(TAG, "========================================");

    wifi_mgr_init();
    web_server_start();
    net_bridge_init();
    usb_rndis_host_init();

    ESP_LOGI(TAG, "System ready");

    while (1) {
        wifi_mgr_task();
        usb_rndis_host_task();
        net_bridge_task();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
