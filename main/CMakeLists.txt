idf_component_register(SRCS "main.c" "wifi_manager.c" "web_server.c" "usb_rndis_host.c" "net_bridge.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_http_server esp_netif driver)
