#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"
#include "config.h"
#include "wifi_manager.h"

static const char *TAG = "WIFI";

static app_wifi_mode_t s_mode = WIFI_MODE_AP_ONLY;
static bool s_ap_started = false;
static bool s_sta_connected = false;
static char s_sta_ssid[32] = {0};
static char s_sta_pass[64] = {0};
static int s_retry_count = 0;
static char s_last_scan_err[64] = "";

static wifi_ap_record_t s_scan_records[20];
static uint16_t s_scan_count = 0;
static volatile bool s_scan_ready = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "AP: station connected");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "AP: station disconnected");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (strlen(s_sta_ssid) > 0) {
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        if (s_retry_count < 5) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "STA retry connect...");
        } else {
            ESP_LOGW(TAG, "STA connect failed after retries");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_connected = true;
        s_retry_count = 0;
    }
}

esp_err_t wifi_mgr_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    /* Load STA config from NVS */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t len = sizeof(s_sta_ssid);
        nvs_get_str(nvs_handle, NVS_KEY_STA_SSID, s_sta_ssid, &len);
        len = sizeof(s_sta_pass);
        nvs_get_str(nvs_handle, NVS_KEY_STA_PASS, s_sta_pass, &len);
        nvs_close(nvs_handle);
        if (strlen(s_sta_ssid) > 0) {
            ESP_LOGI(TAG, "Loaded STA config: %s", s_sta_ssid);
        }
    }

    /* Set mode to APSTA - same as Arduino WiFi.mode(WIFI_AP_STA) */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* Configure AP */
    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, DEFAULT_AP_SSID, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(DEFAULT_AP_SSID);
    ap_config.ap.channel = AP_CHANNEL;
    strncpy((char *)ap_config.ap.password, DEFAULT_AP_PASSWORD, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.max_connection = AP_MAX_STA;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;
    if (strlen(DEFAULT_AP_PASSWORD) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    /* Configure STA if saved */
    if (strlen(s_sta_ssid) > 0) {
        wifi_config_t sta_config = {0};
        strncpy((char *)sta_config.sta.ssid, s_sta_ssid, sizeof(sta_config.sta.ssid) - 1);
        strncpy((char *)sta_config.sta.password, s_sta_pass, sizeof(sta_config.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    }

    /* Start WiFi ONCE - Arduino does this internally */
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Set AP IP */
    esp_netif_ip_info_t ip_info;
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);

    s_ap_started = true;
    s_mode = WIFI_MODE_AP_STA;
    ESP_LOGI(TAG, "WiFi init complete, mode=APSTA");
    return ESP_OK;
}

static void scan_task(void *pvParameters)
{
    (void)pvParameters;
    s_last_scan_err[0] = '\0';
    s_scan_count = 0;
    memset(s_scan_records, 0, sizeof(s_scan_records));

    ESP_LOGI(TAG, "Scan task starting...");

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    ESP_LOGI(TAG, "esp_wifi_scan_start returned: %s", esp_err_to_name(err));
    if (err == ESP_OK) {
        esp_wifi_scan_get_ap_num(&s_scan_count);
        if (s_scan_count > 20) s_scan_count = 20;
        uint16_t num = s_scan_count;
        esp_err_t get_err = esp_wifi_scan_get_ap_records(&num, s_scan_records);
        ESP_LOGI(TAG, "get_ap_records: %s, num=%d", esp_err_to_name(get_err), num);
        ESP_LOGI(TAG, "Scan done, found %d APs", s_scan_count);
    } else {
        snprintf(s_last_scan_err, sizeof(s_last_scan_err), "%s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Scan failed: %s", s_last_scan_err);
        s_scan_count = 0;
    }

    s_scan_ready = true;
    vTaskDelete(NULL);
}

void wifi_mgr_task(void)
{
}

app_wifi_mode_t wifi_mgr_get_mode(void) { return s_mode; }
bool wifi_mgr_is_ap_started(void) { return s_ap_started; }
bool wifi_mgr_is_sta_connected(void) { return s_sta_connected; }
int wifi_mgr_get_ap_sta_num(void)
{
    wifi_sta_list_t sta_list;
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    if (err == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}
const char *wifi_mgr_get_sta_ssid(void) { return s_sta_ssid; }
const char *wifi_mgr_get_last_scan_err(void) { return s_last_scan_err; }

esp_err_t wifi_mgr_set_sta_config(const char *ssid, const char *pass)
{
    strncpy(s_sta_ssid, ssid, sizeof(s_sta_ssid) - 1);
    strncpy(s_sta_pass, pass, sizeof(s_sta_pass) - 1);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, NVS_KEY_STA_SSID, s_sta_ssid);
        nvs_set_str(nvs_handle, NVS_KEY_STA_PASS, s_sta_pass);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    /* Reconfigure STA without stop/start */
    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, s_sta_ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char *)sta_config.sta.password, s_sta_pass, sizeof(sta_config.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_connect();
    return ESP_OK;
}

esp_err_t wifi_mgr_clear_sta_config(void)
{
    s_sta_ssid[0] = '\0';
    s_sta_pass[0] = '\0';
    s_sta_connected = false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_KEY_STA_SSID);
        nvs_erase_key(nvs_handle, NVS_KEY_STA_PASS);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    esp_wifi_disconnect();
    wifi_config_t sta_config = {0};
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    return ESP_OK;
}

esp_err_t wifi_mgr_scan(wifi_ap_record_t *ap_records, uint16_t *ap_count, uint16_t max_records)
{
    if (!ap_records || !ap_count) return ESP_ERR_INVALID_ARG;

    s_scan_ready = false;
    s_scan_count = 0;
    memset(s_scan_records, 0, sizeof(s_scan_records));

    if (xTaskCreate(scan_task, "wifi_scan", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scan task");
        return ESP_ERR_NO_MEM;
    }

    /* Poll for completion (max 15s) */
    for (int i = 0; i < 150; i++) {
        if (s_scan_ready) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!s_scan_ready) {
        ESP_LOGW(TAG, "Scan timeout");
        esp_wifi_scan_stop();
        snprintf(s_last_scan_err, sizeof(s_last_scan_err), "timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t n = (max_records < s_scan_count) ? max_records : s_scan_count;
    if (n > 0) {
        memcpy(ap_records, s_scan_records, n * sizeof(wifi_ap_record_t));
    }
    *ap_count = n;
    return ESP_OK;
}
