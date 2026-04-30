#include <string.h>
#include <stdlib.h>
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

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html><html lang=zh><head>"
        "<meta charset=UTF-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>ESP32-S3 RNDIS Bridge</title>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font:14px/1.5 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh}"
        ".header{background:#16213e;padding:16px;text-align:center;border-bottom:2px solid #0f3460}"
        ".header h1{font-size:18px;color:#e94560;margin-bottom:2px}"
        ".header span{font-size:11px;color:#888}"
        ".card{background:#16213e;margin:12px 16px;border-radius:10px;padding:16px;box-shadow:0 2px 8px rgba(0,0,0,.3)}"
        ".card h2{font-size:14px;color:#e94560;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #0f3460}"
        ".status-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
        ".status-item{background:#0f3460;border-radius:6px;padding:8px 10px}"
        ".status-item .lb{font-size:10px;color:#888;text-transform:uppercase}"
        ".status-item .vl{font-size:13px;color:#fff;word-break:break-all}"
        ".btn{display:inline-block;padding:10px 20px;border:none;border-radius:6px;font-size:13px;cursor:pointer;transition:all .2s;font-weight:600}"
        ".btn-scan{background:#e94560;color:#fff;width:100%}"
        ".btn-scan:hover{background:#d63851}"
        ".btn-scan:disabled{background:#555;cursor:not-allowed}"
        ".btn-connect{background:#0f3460;color:#e94560;width:100%;margin-top:8px}"
        ".btn-connect:hover{background:#1a4a8a}"
        ".btn-connect:disabled{background:#333;color:#666}"
        ".list{border-radius:6px;overflow:hidden;max-height:260px;overflow-y:auto}"
        ".list .item{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:#0f3460;border-bottom:1px solid #16213e;cursor:pointer;transition:background .2s}"
        ".list .item:hover{background:#1a4a8a}"
        ".list .item:last-child{border:none}"
        ".list .item .ssid{font-weight:600;font-size:13px}"
        ".list .item .rssi{font-size:11px;color:#888}"
        ".list .item .lock{font-size:11px;margin-left:6px}"
        ".list .empty{text-align:center;padding:30px;color:#666;font-size:13px}"
        ".input-row{margin-bottom:10px}"
        ".input-row label{display:block;font-size:11px;color:#888;margin-bottom:4px}"
        ".input-row input{width:100%;padding:10px;border-radius:6px;border:1px solid #0f3460;background:#1a1a2e;color:#fff;font-size:13px;outline:none}"
        ".input-row input:focus{border-color:#e94560}"
        ".toast{position:fixed;top:16px;left:50%;transform:translateX(-50%);background:#333;color:#fff;padding:10px 20px;border-radius:6px;font-size:12px;z-index:999;opacity:0;transition:opacity .3s}"
        ".toast.show{opacity:1}"
        ".empty-state{text-align:center;padding:40px 20px;color:#666}"
        ".empty-state .ic{font-size:32px;margin-bottom:8px}"
        ".spin{display:inline-block;width:16px;height:16px;border:2px solid #fff;border-radius:50%;border-top-color:transparent;animation:sp .6s linear infinite;vertical-align:middle;margin-right:6px}"
        "@keyframes sp{to{transform:rotate(360deg)}}"
        ".footer{text-align:center;padding:16px;color:#555;font-size:11px}"
        ".tabs{display:flex;border-radius:8px;overflow:hidden;margin-bottom:12px}"
        ".tab{flex:1;padding:10px;text-align:center;background:#0f3460;color:#888;font-size:12px;cursor:pointer;transition:all .2s}"
        ".tab.on{background:#e94560;color:#fff}"
        ".tab:first-child{border-radius:8px 0 0 8px}"
        ".tab:last-child{border-radius:0 8px 8px 0}"
        "#rndis-ifs{display:none}"
        "</style></head><body>"
        "<div class=header><h1>ESP32-S3 RNDIS Bridge</h1><span id=ver>WiFi &middot; USB RNDIS</span></div>"
        "<div class=card><h2>系统状态 <button class=btn style='padding:4px 12px;font-size:11px;float:right;background:#0f3460;color:#888' onclick=rs()>刷新</button></h2>"
        "<div class=status-grid id=st></div></div>"
        "<div class=card>"
        "<div class=tabs><div class='tab on' id=tb-scan onclick=st('scan')>扫描 WiFi</div><div class=tab id=tb-conn onclick=st('conn')>连接网络</div></div>"
        "<div id=tab-scan><div class=list id=sl><div class=empty-state><div class=ic>&#128269;</div><p>点击扫描按钮搜索 WiFi</p></div></div>"
        "<button class='btn btn-scan' id=sb onclick=sc()>扫描 WiFi</button></div>"
        "<div id=tab-conn style=display:none>"
        "<div class=input-row><label>SSID</label><input type=text id=cs placeholder='选择WiFi网络' readonly></div>"
        "<div class=input-row><label>密码</label><input type=password id=cp placeholder='输入WiFi密码'></div>"
        "<div style='display:flex;gap:8px;margin-top:12px'><button class='btn btn-connect' id=cb onclick=co() style=flex:1>连接</button>"
        "<button class='btn btn-connect' id=db onclick=dc() style='flex:1;background:#555;color:#fff'>断开</button></div>"
        "<div id=cm style='margin-top:10px;font-size:12px;text-align:center;color:#888'></div></div>"
        "</div>"
        "<div class=toast id=t></div><div class=footer>ESP32-S3 RNDIS WiFi Bridge</div>"
        "<script>"
        "var slf,scn,css,con='',sc='',ci=0;"
        "function t(m){var e=document.getElementById('t');e.textContent=m;e.classList.add('show');setTimeout(function(){e.classList.remove('show')},2000)}"
        "function st(v){document.querySelectorAll('.tab').forEach(function(e){e.classList.remove('on')});"
        "document.getElementById('tab-scan').style.display=v==='scan'?'':'none';"
        "document.getElementById('tab-conn').style.display=v==='conn'?'':'none';"
        "document.getElementById(v==='scan'?'tb-scan':'tb-conn').classList.add('on')}"
        "function sc(){var b=document.getElementById('sb'),l=document.getElementById('sl');"
        "b.disabled=true;b.innerHTML='<span class=spin></span>扫描中...';"
        "l.innerHTML='<div class=empty-state><div class=ic>&#128269;</div><p>正在扫描...</p></div>';"
        "var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
        "if(x.status==200){try{var j=JSON.parse(x.responseText);sr(j.networks||[])}catch(e){l.innerHTML='<div class=empty-state><p>解析失败</p></div>'}}"
        "else{l.innerHTML='<div class=empty-state><p>扫描失败:'+x.status+'</p></div>'}"
        "b.disabled=false;b.textContent='重新扫描'}};x.open('GET','/scan',true);x.send()}"
        "function sr(ns){var l=document.getElementById('sl');if(!ns.length){l.innerHTML='<div class=empty-state><div class=ic>&#9888;</div><p>未发现 WiFi 网络</p></div>';return}"
        "var h='';ns.forEach(function(n){var si=n.rssi>-50?'&#128246;':n.rssi>-70?'&#128247;':'&#128248;';"
        "var lk=n.secure=='true'?'&#128274;':'';"
        "h+='<div class=item onclick=sel(\"'+n.ssid+'\",\"'+n.secure+'\")><div><span>'+si+'</span><span class=ssid>'+n.ssid+'</span><span class=lock>'+lk+'</span></div><span class=rssi>'+n.rssi+' dBm</span></div>'}"
        "l.innerHTML=h}"
        "function sel(ss,se){scn=ss;document.getElementById('cs').value=ss;"
        "document.getElementById('cp').value='';"
        "document.getElementById('cp').placeholder=se=='false'?'开放网络，无需密码':'输入WiFi密码';"
        "document.getElementById('cm').textContent='';st('conn')}"
        "function co(){var s=document.getElementById('cs').value,p=document.getElementById('cp').value;"
        "if(!s){t('请先选择 WiFi 网络');return}"
        "var b=document.getElementById('cb');b.disabled=true;b.textContent='连接中...';"
        "document.getElementById('cm').textContent='';"
        "var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
        "if(x.status==200){con=s;document.getElementById('cm').textContent=x.responseText;document.getElementById('cm').style.color='#4caf50';rs()}"
        "else{document.getElementById('cm').textContent='连接失败';document.getElementById('cm').style.color='#f44336'}"
        "b.disabled=false;b.textContent='连接'}};"
        "x.open('POST','/connect',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
        "x.send('ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))}"
        "function dc(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
        "con='';document.getElementById('cm').textContent=x.responseText;document.getElementById('cm').style.color='#888';rs()}};"
        "x.open('POST','/disconnect',true);x.send()}"
        "function rs(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4&&x.status==200){"
        "try{rds(JSON.parse(x.responseText))}catch(e){}}}"
        "x.open('GET','/status',true);x.send()}"
        "function rds(s){var i=[{k:'WiFi '+String.fromCharCode(27169)+String.fromCharCode(24335),v:s.mode},"
        "{k:'AP '+String.fromCharCode(29366)+String.fromCharCode(24577),v:s.ap+' ('+s.ap_ip+')'},"
        "{k:'AP '+String.fromCharCode(23458)+String.fromCharCode(25143)+String.fromCharCode(31471),v:s.ap_cli+' '+String.fromCharCode(20010)},"
        "{k:'STA '+String.fromCharCode(36830)+String.fromCharCode(25509),v:s.sta+(s.sta_ssid?' ['+s.sta_ssid+']':'')},"
        "{k:'STA IP',v:s.sta_ip},{k:'RNDIS '+String.fromCharCode(29366)+String.fromCharCode(24577),v:s.rndis},"
        "{k:'RNDIS '+String.fromCharCode(23601)+String.fromCharCode(32490),v:s.rndis_r?String.fromCharCode(26159):String.fromCharCode(21542)},"
        "{k:'USB '+String.fromCharCode(35774)+String.fromCharCode(22791),v:'VID:'+s.usb_vid+' PID:'+s.usb_pid},"
        "{k:'USB MAC',v:s.usb_mac}];var h='';i.forEach(function(d){h+='<div class=status-item><div class=lb>'+d.k+'</div><div class=vl>'+d.v+'</div></div>'});"
        "document.getElementById('st').innerHTML=h}"
        "setInterval(rs,5000);rs()"
        "</script></body></html>";

    ESP_LOGI(TAG, "root handler, html len=%d", strlen(html));
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "scan_get_handler called");
    static wifi_ap_record_t s_ap_info[20];
    uint16_t ap_count = 0;
    memset(s_ap_info, 0, sizeof(s_ap_info));

    esp_err_t err = wifi_mgr_scan(s_ap_info, &ap_count, 20);
    ESP_LOGI(TAG, "scan result: err=%s, count=%d", esp_err_to_name(err), ap_count);
    const char *dbg = wifi_mgr_get_last_scan_err();

    static char s_resp[4096];
    int pos = 0;
    pos += snprintf(s_resp + pos, sizeof(s_resp) - pos, "{\"networks\":[");
    for (int i = 0; i < ap_count; i++) {
        if (i > 0) pos += snprintf(s_resp + pos, sizeof(s_resp) - pos, ",");
        pos += snprintf(s_resp + pos, sizeof(s_resp) - pos,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
            s_ap_info[i].ssid,
            s_ap_info[i].rssi,
            s_ap_info[i].authmode != WIFI_AUTH_OPEN ? "true" : "false");
    }
    pos += snprintf(s_resp + pos, sizeof(s_resp) - pos, "],\"error\":\"%s\",\"debug\":\"%s\"}",
        err != ESP_OK ? "扫描失败" : "",
        dbg);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s_resp);
    return ESP_OK;
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char buf[128];
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
        strncpy(pass, pass_ptr, sizeof(pass) - 1);
    }

    esp_err_t err = wifi_mgr_set_sta_config(ssid, pass);
    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "配置已保存，正在连接...");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed");
    }
    return ESP_OK;
}

static esp_err_t disconnect_post_handler(httpd_req_t *req)
{
    wifi_mgr_clear_sta_config();
    httpd_resp_sendstr(req, "已断开并清除配置");
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "status_get_handler called");
    char resp[1024];
    int pos = 0;

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
    char mac_str[18] = "N/A";
    if (usb_rndis_host_is_connected() && mac[0] != 0) {
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    pos += snprintf(resp + pos, sizeof(resp) - pos,
        "{\"mode\":\"%s\",\"ap\":\"%s\",\"ap_ip\":\"%s\",\"ap_cli\":%d,"
        "\"sta\":\"%s\",\"sta_ip\":\"%s\",\"sta_ssid\":\"%s\","
        "\"rndis\":\"%s\",\"rndis_r\":%s,"
        "\"usb_vid\":\"%04X\",\"usb_pid\":\"%04X\",\"usb_mac\":\"%s\"}",
        wifi_mgr_get_mode() == WIFI_MODE_AP_ONLY ? "AP_ONLY" :
        (wifi_mgr_get_mode() == WIFI_MODE_AP_STA ? "AP_STA" : "STA_ONLY"),
        wifi_mgr_is_ap_started() ? "Started" : "Stopped",
        ap_ip,
        wifi_mgr_get_ap_sta_num(),
        wifi_mgr_is_sta_connected() ? "Connected" : "Disconnected",
        sta_ip,
        wifi_mgr_get_sta_ssid(),
        usb_rndis_host_state_str(),
        usb_rndis_host_is_ready() ? "true" : "false",
        usb_rndis_host_get_vid(),
        usb_rndis_host_get_pid(),
        mac_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
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

static const httpd_uri_t uri_status = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
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
    httpd_register_uri_handler(server, &uri_status);

    ESP_LOGI(TAG, "Web server started on port %d", WEB_SERVER_PORT);
    return ESP_OK;
}
