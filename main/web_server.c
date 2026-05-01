#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "config.h"
#include "wifi_manager.h"
#include "usb_rndis_host.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "WEB";

#define CSS \
    "*{margin:0;padding:0;box-sizing:border-box}" \
    "body{font:14px/1.5 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh}" \
    ".h{background:#16213e;padding:16px;text-align:center;border-bottom:2px solid #0f3460}" \
    ".h h1{font-size:18px;color:#e94560;margin-bottom:2px}" \
    ".h span{font-size:11px;color:#888}" \
    ".c{background:#16213e;margin:12px 16px;border-radius:10px;padding:16px}" \
    ".c h2{font-size:14px;color:#e94560;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #0f3460}" \
    ".sg{display:grid;grid-template-columns:1fr 1fr;gap:8px}" \
    ".si{background:#0f3460;border-radius:6px;padding:8px 10px}" \
    ".si .lb{font-size:10px;color:#888;text-transform:uppercase}" \
    ".si .vl{font-size:13px;color:#fff;word-break:break-all}" \
    "button,.btn{display:inline-block;padding:10px 20px;border:none;border-radius:6px;font-size:13px;cursor:pointer;font-weight:600;text-decoration:none;text-align:center}" \
    ".btn-red{background:#e94560;color:#fff;width:100%}" \
    ".btn-red:hover{background:#d63851}" \
    ".btn-blue{background:#0f3460;color:#e94560;width:100%;margin-top:8px}" \
    ".btn-blue:hover{background:#1a4a8a}" \
    ".btn-gray{background:#555;color:#fff}" \
    ".ir{margin-bottom:10px}" \
    ".ir label{display:block;font-size:11px;color:#888;margin-bottom:4px}" \
    ".ir input{width:100%;padding:10px;border-radius:6px;border:1px solid #0f3460;background:#1a1a2e;color:#fff;font-size:13px;outline:none}" \
    ".ir input:focus{border-color:#e94560}" \
    ".li{border-radius:6px;overflow:hidden}" \
    ".li form{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:#0f3460;border-bottom:1px solid #16213e}" \
    ".li form:last-child{border:none}" \
    ".li .nm{font-weight:600;font-size:13px}" \
    ".li .db{font-size:11px;color:#888}" \
    ".li .act{background:#e94560;color:#fff;padding:6px 12px;border-radius:4px;font-size:11px;border:none;cursor:pointer}" \
    ".empty{text-align:center;padding:40px 20px;color:#666}" \
    ".msg{padding:12px;border-radius:6px;text-align:center;font-size:13px;margin-bottom:12px}" \
    ".msg-ok{background:#1b3a1b;color:#4caf50}" \
    ".msg-err{background:#3a1b1b;color:#f44336}" \
    ".f{text-align:center;padding:16px;color:#555;font-size:11px}"

#define PAGE_START \
    "<!DOCTYPE html><html lang=zh><head>" \
    "<meta charset=UTF-8>" \
    "<meta name=viewport content='width=device-width,initial-scale=1'>" \
    "<title>ESP32-S3 RNDIS Bridge</title>" \
    "<style>" CSS "</style></head><body>" \
    "<div class=h><h1>ESP32-S3 RNDIS Bridge</h1><span>WiFi &middot; USB RNDIS</span></div>"

#define WRITE_STATIC(buf, size, pos, str) do { \
    size_t _len = strlen(str); \
    if ((size_t)(pos) + _len < (size_t)(size)) { \
        memcpy((buf) + (pos), (str), _len); \
        (pos) += _len; \
    } \
} while(0)

#define PAGE_END "</body></html>"

static void build_status_html(char *buf, size_t size, int *pos)
{
    esp_netif_ip_info_t ip_info;
    char ap_ip[16] = "0.0.0.0";
    char sta_ip[16] = "0.0.0.0";

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif && esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        snprintf(ap_ip, sizeof(ap_ip), IPSTR, IP2STR(&ip_info.ip));
    }
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&ip_info.ip));
    }

    const uint8_t *mac = usb_rndis_host_get_mac();
    char mac_str[24] = "N/A";
    if (usb_rndis_host_is_connected() && mac[0] != 0) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    int p = *pos;
    p += snprintf(buf + p, size - p,
        "<div class=c><h2>系统状态 <a href=/ class=btn style='padding:4px 12px;font-size:11px;float:right;background:#0f3460;color:#888;text-decoration:none'>刷新</a></h2>"
        "<div class=sg>");
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>WiFi 模式</div><div class=vl>%s</div></div>",
        wifi_mgr_get_mode() == WIFI_MODE_AP_ONLY ? "AP" :
        (wifi_mgr_get_mode() == WIFI_MODE_AP_STA ? "AP+STA" : "STA"));
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>AP 状态</div><div class=vl>%s (%s)</div></div>",
        wifi_mgr_is_ap_started() ? "运行中" : "未启动", ap_ip);
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>AP 客户端</div><div class=vl>%d 个</div></div>",
        wifi_mgr_get_ap_sta_num());
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>STA 连接</div><div class=vl>%s%s%s</div></div>",
        wifi_mgr_is_sta_connected() ? "已连接" : "未连接",
        wifi_mgr_is_sta_connected() ? " [" : "",
        wifi_mgr_get_sta_ssid());
    p += snprintf(buf + p, size - p, "%s</div></div>",
        wifi_mgr_is_sta_connected() ? "]" : "");
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>STA IP</div><div class=vl>%s</div></div>", sta_ip);
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>RNDIS 状态</div><div class=vl>%s</div></div>",
        usb_rndis_host_state_str());
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>RNDIS 就绪</div><div class=vl>%s</div></div>",
        usb_rndis_host_is_ready() ? "是" : "否");
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>USB 设备</div><div class=vl>VID:%04X PID:%04X</div></div>",
        (unsigned int)usb_rndis_host_get_vid(), (unsigned int)usb_rndis_host_get_pid());
    p += snprintf(buf + p, size - p,
        "<div class=si><div class=lb>USB MAC</div><div class=vl>%s</div></div>", mac_str);
    p += snprintf(buf + p, size - p, "</div></div>");
    *pos = p;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char html[4096];
    int pos = 0;

    WRITE_STATIC(html, sizeof(html), pos, PAGE_START);

    if (req->user_ctx) {
        pos += snprintf(html + pos, sizeof(html) - pos,
            "<div class='msg msg-ok'>%s</div>", (const char *)req->user_ctx);
    }

    build_status_html(html, sizeof(html), &pos);

    /* Scan form */
    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div class=c><h2>WiFi 扫描</h2>"
        "<p style=margin-bottom:12px;color:#888>搜索附近的 WiFi 网络</p>"
        "<form method=GET action=/scan>"
        "<button class='btn btn-red' type=submit>扫描 WiFi</button>"
        "</form></div>");

    /* Connect form */
    const char *cur_ssid = wifi_mgr_get_sta_ssid();
    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div class=c><h2>连接网络</h2>"
        "<form method=POST action=/connect>"
        "<div class=ir><label>SSID</label>"
        "<input type=text name=ssid placeholder='输入 WiFi 名称' value='%s'></div>"
        "<div class=ir><label>密码</label>"
        "<input type=password name=pass placeholder='输入 WiFi 密码'></div>"
        "<button class='btn btn-blue' type=submit>连接</button>"
        "</form>"
        "<form method=POST action=/disconnect style=margin-top:8px>"
        "<button class='btn btn-gray' type=submit>断开当前连接</button>"
        "</form></div>",
        cur_ssid);

    WRITE_STATIC(html, sizeof(html), pos, PAGE_END);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    static wifi_ap_record_t s_ap_info[20];
    uint16_t ap_count = 0;
    memset(s_ap_info, 0, sizeof(s_ap_info));

    esp_err_t err = wifi_mgr_scan(s_ap_info, &ap_count, 20);
    const char *dbg = wifi_mgr_get_last_scan_err();

    char html[4096];
    int pos = 0;
    WRITE_STATIC(html, sizeof(html), pos, PAGE_START);

    /* Back link */
    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div style='padding:12px 16px'><a href=/ style=color:#e94560;text-decoration:none>"
        "&larr; 返回</a></div>");

    /* Results */
    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div class=c><h2>扫描结果</h2>");

    if (err != ESP_OK || ap_count == 0) {
        pos += snprintf(html + pos, sizeof(html) - pos,
            "<div class=empty><p>%s</p><p style=font-size:11px;color:#555>%s</p>"
            "<a href=/scan class='btn btn-red' style=margin-top:12px>重新扫描</a></div>",
            ap_count == 0 ? "未发现 WiFi 网络" : "扫描失败", dbg);
    } else {
        pos += snprintf(html + pos, sizeof(html) - pos,
            "<p style=color:#888;font-size:12px;margin-bottom:12px>发现 %d 个网络</p>"
            "<div class=li>", ap_count);
        for (int i = 0; i < ap_count; i++) {
            const char *sig = s_ap_info[i].rssi > -50 ? "[强]" :
                              s_ap_info[i].rssi > -70 ? "[中]" : "[弱]";
            const char *lk = s_ap_info[i].authmode != WIFI_AUTH_OPEN ? " \xF0\x9F\x94\x92" : "";
            pos += snprintf(html + pos, sizeof(html) - pos,
                "<form method=POST action=/connect style='display:flex;align-items:center;"
                "justify-content:space-between;padding:10px 12px;background:#0f3460;"
                "border-bottom:1px solid #16213e'>"
                "<div><span class=nm>%s</span>%s <span style=font-size:11px;color:#888>%s</span></div>"
                "<span class=db style=margin-right:8px>%d dBm</span>"
                "<input type=hidden name=ssid value='%s'>"
                "<input type=hidden name=pass value=''>"
                "<button class=act type=submit>连接</button>"
                "</form>",
                s_ap_info[i].ssid, lk, sig,
                s_ap_info[i].rssi,
                s_ap_info[i].ssid);
        }
        pos += snprintf(html + pos, sizeof(html) - pos, "</div>");
    }

    pos += snprintf(html + pos, sizeof(html) - pos, "</div>");
    WRITE_STATIC(html, sizeof(html), pos, PAGE_END);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "pass=");
    if (ssid_ptr) {
        ssid_ptr += 5;
        char *end = strchr(ssid_ptr, '&');
        if (end) *end = '\0';
        strncpy(ssid, ssid_ptr, sizeof(ssid) - 1);
    }
    if (pass_ptr) {
        pass_ptr += 5;
        char *end = strchr(pass_ptr, '&');
        if (end) *end = '\0';
        strncpy(pass, pass_ptr, sizeof(pass) - 1);
    }

    esp_err_t err = wifi_mgr_set_sta_config(ssid, pass);

    char html[4096];
    int pos = 0;
    WRITE_STATIC(html, sizeof(html), pos, PAGE_START);

    if (err == ESP_OK) {
        pos += snprintf(html + pos, sizeof(html) - pos,
            "<div style='margin:24px 16px'><div class='msg msg-ok'>"
            "已保存配置，正在连接 %s...</div>", ssid);
    } else {
        pos += snprintf(html + pos, sizeof(html) - pos,
            "<div style='margin:24px 16px'><div class='msg msg-err'>连接失败</div>");
    }

    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div style='text-align:center;margin:12px'>"
        "<a href='/' class=btn style='background:#0f3460;color:#e94560;text-decoration:none'>返回首页</a>"
        "</div></div>");
    WRITE_STATIC(html, sizeof(html), pos, PAGE_END);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static esp_err_t disconnect_post_handler(httpd_req_t *req)
{
    wifi_mgr_clear_sta_config();

    char html[4096];
    int pos = 0;
    WRITE_STATIC(html, sizeof(html), pos, PAGE_START);
    pos += snprintf(html + pos, sizeof(html) - pos,
        "<div style='margin:24px 16px'><div class='msg msg-ok'>已断开连接并清除配置</div>"
        "<div style='text-align:center;margin:12px'>"
        "<a href='/' class=btn style='background:#0f3460;color:#e94560;text-decoration:none'>返回首页</a>"
        "</div></div>");
    WRITE_STATIC(html, sizeof(html), pos, PAGE_END);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static const httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_scan = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_connect = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_disconnect = {
    .uri = "/disconnect",
    .method = HTTP_POST,
    .handler = disconnect_post_handler,
    .user_ctx = NULL
};

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.stack_size = 12288;
    config.recv_wait_timeout = 20;
    config.send_wait_timeout = 20;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return ret;
    }

    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_scan);
    httpd_register_uri_handler(server, &uri_connect);
    httpd_register_uri_handler(server, &uri_disconnect);

    ESP_LOGI(TAG, "Web server started on port %d", WEB_SERVER_PORT);
    return ESP_OK;
}
