#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/lwip_napt.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "config.h"
#include "usb_rndis_host.h"
#include "net_bridge.h"

static const char *TAG = "BRIDGE";
static bool s_nat_enabled = false;
static struct netif s_rndis_netif;
static bool s_rndis_netif_added = false;

static err_t rndis_netif_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    (void)ipaddr;
    if (!usb_rndis_host_is_ready()) return ERR_IF;

    uint8_t eth_frame[RNDIS_MAX_TRANSFER_SIZE];
    size_t eth_len = 0;

    memcpy(eth_frame, usb_rndis_host_get_mac(), 6);
    memcpy(eth_frame + 6, netif->hwaddr, 6);
    uint16_t ethertype = PP_HTONS(ETHTYPE_IP);
    memcpy(eth_frame + 12, &ethertype, 2);
    eth_len = 14;

    if (p->tot_len > (RNDIS_MAX_TRANSFER_SIZE - 14)) {
        return ERR_BUF;
    }

    pbuf_copy_partial(p, eth_frame + 14, p->tot_len, 0);
    eth_len += p->tot_len;

    int ret = usb_rndis_host_send_packet(eth_frame, eth_len);
    return (ret > 0) ? ERR_OK : ERR_IF;
}

static void bridge_setup_rndis_netif(void)
{
    if (s_rndis_netif_added) return;
    if (!usb_rndis_host_is_ready()) return;

    const uint8_t *rndis_mac = usb_rndis_host_get_mac();
    if (rndis_mac[0] == 0) return;

    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192, 168, 42, 2);
    IP4_ADDR(&gw, 192, 168, 42, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);

    struct netif *nif = netif_add(&s_rndis_netif, &ipaddr, &netmask, &gw,
                                  NULL, NULL, NULL);
    if (!nif) {
        ESP_LOGE(TAG, "netif_add failed for RNDIS");
        return;
    }

    netif_create_ip6_linklocal_address(&s_rndis_netif, 1);
    netif_set_up(&s_rndis_netif);
    netif_set_default(&s_rndis_netif);

    s_rndis_netif.output = rndis_netif_output;
    s_rndis_netif.mtu = 1500;
    s_rndis_netif.flags |= NETIF_FLAG_ETHARP;
    s_rndis_netif.flags |= NETIF_FLAG_LINK_UP;

    s_rndis_netif_added = true;
    ESP_LOGI(TAG, "RNDIS netif added, default route set");
}

static void bridge_forward_usb_to_wifi(const uint8_t *data, size_t len)
{
    if (len < 14) return;

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) return;

    esp_err_t err = esp_netif_receive(ap_netif, (void *)(data + 14), len - 14, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_receive failed: %s", esp_err_to_name(err));
    }
}

esp_err_t net_bridge_init(void)
{
    ESP_LOGI(TAG, "Net bridge init");
    return ESP_OK;
}

void net_bridge_task(void)
{
    static uint8_t pkt_buf[RNDIS_MAX_TRANSFER_SIZE];
    int len = usb_rndis_host_receive_packet(pkt_buf, sizeof(pkt_buf));
    if (len > 0) {
        bridge_forward_usb_to_wifi(pkt_buf, (size_t)len);
    }

    if (!s_rndis_netif_added && usb_rndis_host_is_ready()) {
        bridge_setup_rndis_netif();
    }

    if (!s_nat_enabled && s_rndis_netif_added) {
        net_bridge_enable_nat(true);
    }
}

void net_bridge_enable_nat(bool on)
{
    if (!on) {
        s_nat_enabled = false;
        return;
    }

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) {
        ESP_LOGW(TAG, "AP netif not found");
        return;
    }

    struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(ap_netif);
    if (!lwip_netif) {
        ESP_LOGW(TAG, "lwIP netif not available");
        return;
    }

    ip_napt_enable_netif(lwip_netif, 1);
    s_nat_enabled = true;
    ESP_LOGI(TAG, "NAT enabled");
}
