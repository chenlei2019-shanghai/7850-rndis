#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

typedef enum {
    WIFI_MODE_AP_ONLY,
    WIFI_MODE_STA_ONLY,
    WIFI_MODE_AP_STA
} app_wifi_mode_t;

esp_err_t wifi_mgr_init(void);
void wifi_mgr_task(void);

app_wifi_mode_t wifi_mgr_get_mode(void);
bool wifi_mgr_is_ap_started(void);
bool wifi_mgr_is_sta_connected(void);
int wifi_mgr_get_ap_sta_num(void);
const char *wifi_mgr_get_sta_ssid(void);
const char *wifi_mgr_get_last_scan_err(void);

esp_err_t wifi_mgr_scan(wifi_ap_record_t *ap_records, uint16_t *ap_count, uint16_t max_records);

esp_err_t wifi_mgr_set_sta_config(const char *ssid, const char *pass);
esp_err_t wifi_mgr_clear_sta_config(void);

#endif
