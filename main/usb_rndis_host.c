#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"
#include "esp_heap_caps.h"
#include "config.h"
#include "usb_rndis_host.h"

static const char *TAG = "USB_RNDIS";

#define RNDIS_MSG_INIT          0x00000002
#define RNDIS_MSG_INIT_C        0x80000002
#define RNDIS_MSG_SET           0x00000005
#define RNDIS_MSG_SET_C         0x80000005
#define RNDIS_MSG_PACKET        0x00000001
#define RNDIS_STATUS_SUCCESS    0x00000000
#define OID_GEN_CURRENT_PACKET_FILTER   0x0001010E
#define OID_802_3_CURRENT_ADDRESS       0x01010102
#define NDIS_PACKET_TYPE_DIRECTED       0x00000001
#define NDIS_PACKET_TYPE_MULTICAST      0x00000002
#define NDIS_PACKET_TYPE_BROADCAST      0x00000008
#define USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND   0x00
#define USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE   0x01

typedef struct __attribute__((packed)) {
    uint32_t MessageType;
    uint32_t MessageLength;
    uint32_t RequestId;
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint32_t MaxTransferSize;
} rndis_init_msg_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType;
    uint32_t MessageLength;
    uint32_t RequestId;
    uint32_t Status;
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint32_t DeviceFlags;
    uint32_t Medium;
    uint32_t MaxPacketsPerMessage;
    uint32_t MaxTransferSize;
    uint32_t PacketAlignmentFactor;
    uint32_t AfListOffset;
    uint32_t AfListSize;
} rndis_init_cmplt_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType;
    uint32_t MessageLength;
    uint32_t RequestId;
    uint32_t Oid;
    uint32_t InformationBufferLength;
    uint32_t InformationBufferOffset;
    uint32_t DeviceVcHandle;
} rndis_set_msg_t;

typedef struct __attribute__((packed)) {
    uint32_t MessageType;
    uint32_t MessageLength;
    uint32_t DataOffset;
    uint32_t DataLength;
    uint32_t OOBDataOffset;
    uint32_t OOBDataLength;
    uint32_t NumOOBDataElements;
    uint32_t PerPacketInfoOffset;
    uint32_t PerPacketInfoLength;
    uint32_t VcHandle;
    uint32_t Reserved;
} rndis_packet_msg_t;

static rndis_state_t s_state = RNDIS_STATE_DISCONNECTED;
static usb_host_client_handle_t s_client = NULL;
static usb_device_handle_t s_dev = NULL;
static uint8_t s_iface_num = 0xFF;
static uint8_t s_bulk_in_ep = 0;
static uint8_t s_bulk_out_ep = 0;

static usb_transfer_t *s_ctrl_xfer = NULL;
static usb_transfer_t *s_bulk_in_xfer = NULL;
static usb_transfer_t *s_bulk_out_xfer = NULL;

static volatile bool s_ctrl_done = false;
static volatile bool s_bulk_in_done = false;
static volatile bool s_bulk_in_pending = false;
static volatile bool s_bulk_out_done = false;
static volatile bool s_rx_available = false;
static volatile bool s_has_new_device = false;
static volatile bool s_device_gone = false;
static uint8_t s_pending_addr = 0;

static uint16_t s_vid = 0;
static uint16_t s_pid = 0;
static uint8_t s_dev_mac[6] = {0};
static size_t s_last_rx_len = 0;

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;
    if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        s_pending_addr = event_msg->new_dev.address;
        s_has_new_device = true;
    } else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        s_device_gone = true;
    }
}

static void ctrl_xfer_cb(usb_transfer_t *xfer)
{
    (void)xfer;
    s_ctrl_done = true;
}

static void bulk_in_xfer_cb(usb_transfer_t *xfer)
{
    (void)xfer;
    s_bulk_in_done = true;
    s_bulk_in_pending = false;
}

static void bulk_out_xfer_cb(usb_transfer_t *xfer)
{
    (void)xfer;
    s_bulk_out_done = true;
}

esp_err_t usb_rndis_host_init(void)
{
    const usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
        return err;
    }

    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = NULL
        },
        .max_num_event_msg = 8
    };
    err = usb_host_client_register(&client_config, &s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "usb_host_client_register failed: %s", esp_err_to_name(err));
        usb_host_uninstall();
        return err;
    }

    size_t ctrl_size = sizeof(usb_setup_packet_t) + 256;
    err = usb_host_transfer_alloc(ctrl_size, 0, &s_ctrl_xfer);
    if (err != ESP_OK) goto fail;
    s_ctrl_xfer->context = NULL;

    err = usb_host_transfer_alloc(RNDIS_MAX_TRANSFER_SIZE, 0, &s_bulk_in_xfer);
    if (err != ESP_OK) goto fail;
    s_bulk_in_xfer->context = NULL;

    err = usb_host_transfer_alloc(RNDIS_MAX_TRANSFER_SIZE, 0, &s_bulk_out_xfer);
    if (err != ESP_OK) goto fail;
    s_bulk_out_xfer->context = NULL;

    ESP_LOGI(TAG, "USB Host init OK");
    return ESP_OK;

fail:
    if (s_ctrl_xfer) { usb_host_transfer_free(s_ctrl_xfer); s_ctrl_xfer = NULL; }
    if (s_bulk_in_xfer) { usb_host_transfer_free(s_bulk_in_xfer); s_bulk_in_xfer = NULL; }
    if (s_bulk_out_xfer) { usb_host_transfer_free(s_bulk_out_xfer); s_bulk_out_xfer = NULL; }
    usb_host_client_deregister(s_client);
    s_client = NULL;
    usb_host_uninstall();
    return ESP_FAIL;
}

static void close_device(void)
{
    if (s_dev) {
        if (s_iface_num != 0xFF) {
            usb_host_interface_release(s_client, s_dev, s_iface_num);
            s_iface_num = 0xFF;
        }
        usb_host_device_close(s_client, s_dev);
        s_dev = NULL;
    }
    s_bulk_in_ep = 0;
    s_bulk_out_ep = 0;
    s_rx_available = false;
    s_bulk_in_pending = false;
    s_last_rx_len = 0;
}

static bool ctrl_request(uint8_t bm_req_type, uint8_t b_request, uint16_t w_value, uint16_t w_index,
                         uint16_t w_length, const uint8_t *tx_data, uint8_t *rx_data)
{
    if (!s_dev || !s_ctrl_xfer) return false;

    uint8_t *buf = s_ctrl_xfer->data_buffer;
    usb_setup_packet_t *setup = (usb_setup_packet_t *)buf;
    setup->bmRequestType = bm_req_type;
    setup->bRequest = b_request;
    setup->wValue = w_value;
    setup->wIndex = w_index;
    setup->wLength = w_length;

    size_t total_bytes = sizeof(usb_setup_packet_t);
    if (tx_data && w_length > 0) {
        memcpy(buf + sizeof(usb_setup_packet_t), tx_data, w_length);
        total_bytes += w_length;
    } else if ((bm_req_type & 0x80) && w_length > 0) {
        total_bytes += w_length;
    }

    s_ctrl_xfer->num_bytes = total_bytes;
    s_ctrl_xfer->device_handle = s_dev;
    s_ctrl_xfer->callback = ctrl_xfer_cb;
    s_ctrl_xfer->context = NULL;

    s_ctrl_done = false;
    esp_err_t err = usb_host_transfer_submit_control(s_client, s_ctrl_xfer);
    if (err != ESP_OK) return false;

    TickType_t start = xTaskGetTickCount();
    while (!s_ctrl_done && (xTaskGetTickCount() - start < pdMS_TO_TICKS(5000))) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), &flags);
        usb_host_client_handle_events(s_client, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (!s_ctrl_done) {
        ESP_LOGW(TAG, "Control transfer timeout");
        return false;
    }

    if (s_ctrl_xfer->status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_LOGW(TAG, "Control transfer status: %d", s_ctrl_xfer->status);
        return false;
    }

    if (rx_data && w_length > 0 && (bm_req_type & 0x80)) {
        size_t actual = s_ctrl_xfer->actual_num_bytes;
        if (actual > sizeof(usb_setup_packet_t)) {
            size_t data_len = actual - sizeof(usb_setup_packet_t);
            if (data_len > w_length) data_len = w_length;
            memcpy(rx_data, buf + sizeof(usb_setup_packet_t), data_len);
        }
    }
    return true;
}

static bool rndis_set_packet_filter(uint32_t filter)
{
    uint8_t buf[32] = {0};
    rndis_set_msg_t *set_msg = (rndis_set_msg_t *)buf;
    set_msg->MessageType = RNDIS_MSG_SET;
    set_msg->MessageLength = sizeof(rndis_set_msg_t) + 4;
    set_msg->RequestId = 2;
    set_msg->Oid = OID_GEN_CURRENT_PACKET_FILTER;
    set_msg->InformationBufferLength = 4;
    set_msg->InformationBufferOffset = 16;
    set_msg->DeviceVcHandle = 0;
    memcpy(buf + sizeof(rndis_set_msg_t), &filter, 4);

    if (!ctrl_request(0x21, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, s_iface_num,
                      set_msg->MessageLength, buf, NULL)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t resp[32] = {0};
    if (!ctrl_request(0xA1, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, s_iface_num,
                      sizeof(resp), NULL, resp)) {
        return false;
    }
    uint32_t status = *(uint32_t *)(resp + 12);
    if (status != RNDIS_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "SET_CMPLT status=0x%08X", status);
    }
    return true;
}

static bool rndis_initialize(void)
{
    rndis_init_msg_t init_msg = {
        .MessageType = RNDIS_MSG_INIT,
        .MessageLength = sizeof(rndis_init_msg_t),
        .RequestId = 1,
        .MajorVersion = 1,
        .MinorVersion = 0,
        .MaxTransferSize = 0x00004000
    };
    uint8_t tx[sizeof(rndis_init_msg_t)];
    memcpy(tx, &init_msg, sizeof(tx));

    if (!ctrl_request(0x21, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, s_iface_num,
                      sizeof(tx), tx, NULL)) {
        ESP_LOGE(TAG, "RNDIS_INIT_MSG send failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    uint8_t resp[sizeof(rndis_init_cmplt_t) + 16];
    memset(resp, 0, sizeof(resp));
    if (!ctrl_request(0xA1, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, s_iface_num,
                      sizeof(resp), NULL, resp)) {
        ESP_LOGE(TAG, "RNDIS_INIT_CMPLT read failed");
        return false;
    }

    rndis_init_cmplt_t *cmplt = (rndis_init_cmplt_t *)resp;
    if (cmplt->MessageType != RNDIS_MSG_INIT_C || cmplt->Status != RNDIS_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "RNDIS init response: type=0x%08X status=0x%08X", cmplt->MessageType, cmplt->Status);
        if (cmplt->MessageType != RNDIS_MSG_INIT_C) return false;
    }
    ESP_LOGI(TAG, "RNDIS init complete, MaxTransferSize=%d", cmplt->MaxTransferSize);

    if (!rndis_set_packet_filter(NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_MULTICAST)) {
        ESP_LOGE(TAG, "Set packet filter failed");
        return false;
    }

    uint8_t mac_buf[32] = {0};
    uint8_t query[28] = {0};
    *(uint32_t *)(query + 0) = 0x00000004;
    *(uint32_t *)(query + 4) = 24;
    *(uint32_t *)(query + 8) = 3;
    *(uint32_t *)(query + 12) = OID_802_3_CURRENT_ADDRESS;
    if (ctrl_request(0x21, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, s_iface_num, 24, query, NULL)) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (ctrl_request(0xA1, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, s_iface_num, sizeof(mac_buf), NULL, mac_buf)) {
            memcpy(s_dev_mac, mac_buf + 22, 6);
        }
    }

    return true;
}

static bool open_device(void)
{
    esp_err_t err = usb_host_device_open(s_client, s_pending_addr, &s_dev);
    if (err != ESP_OK) return false;

    const usb_device_desc_t *dev_desc;
    err = usb_host_get_device_descriptor(s_dev, &dev_desc);
    if (err != ESP_OK) { close_device(); return false; }

    s_vid = dev_desc->idVendor;
    s_pid = dev_desc->idProduct;

    bool known = false;
    for (size_t i = 0; i < RNDIS_DEVICE_COUNT; i++) {
        if (RNDIS_DEVICE_TABLE[i].vid == s_vid && RNDIS_DEVICE_TABLE[i].pid == s_pid) {
            known = true;
            ESP_LOGI(TAG, "Known device: %s", RNDIS_DEVICE_TABLE[i].name);
            break;
        }
    }
    if (!known) {
        ESP_LOGI(TAG, "Unknown VID/PID %04X:%04X, try class match", s_vid, s_pid);
    }

    const usb_config_desc_t *cfg_desc;
    err = usb_host_get_active_config_descriptor(s_dev, &cfg_desc);
    if (err != ESP_OK) { close_device(); return false; }

    const uint8_t *ptr = (const uint8_t *)cfg_desc + cfg_desc->bLength;
    const uint8_t *end = (const uint8_t *)cfg_desc + cfg_desc->wTotalLength;

    uint8_t candidate_iface = 0xFF;
    uint8_t candidate_alt = 0;

    while (ptr < end) {
        const usb_standard_desc_t *std = (const usb_standard_desc_t *)ptr;
        if (std->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)ptr;
            if (intf->bInterfaceClass == USB_CLASS_COMM || intf->bInterfaceClass == 0xEF) {
                candidate_iface = intf->bInterfaceNumber;
                candidate_alt = intf->bAlternateSetting;
                ESP_LOGI(TAG, "Candidate iface %d class=0x%02X", candidate_iface, intf->bInterfaceClass);
            }
        }
        ptr += std->bLength;
    }

    if (candidate_iface == 0xFF) {
        ESP_LOGE(TAG, "No CDC/RNDIS interface found");
        close_device();
        return false;
    }

    err = usb_host_interface_claim(s_client, s_dev, candidate_iface, candidate_alt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Interface claim failed");
        close_device();
        return false;
    }
    s_iface_num = candidate_iface;

    ptr = (const uint8_t *)cfg_desc + cfg_desc->bLength;
    while (ptr < end) {
        const usb_standard_desc_t *std = (const usb_standard_desc_t *)ptr;
        if (std->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)ptr;
            uint8_t attr = ep->bmAttributes;
            if ((attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK) {
                if (USB_EP_DESC_GET_EP_DIR(ep)) {
                    if (!s_bulk_in_ep) s_bulk_in_ep = ep->bEndpointAddress;
                } else {
                    if (!s_bulk_out_ep) s_bulk_out_ep = ep->bEndpointAddress;
                }
            }
        }
        ptr += std->bLength;
    }

    if (!s_bulk_in_ep || !s_bulk_out_ep) {
        ESP_LOGE(TAG, "Bulk endpoints not found");
        close_device();
        return false;
    }
    ESP_LOGI(TAG, "EP IN=0x%02X OUT=0x%02X", s_bulk_in_ep, s_bulk_out_ep);
    return true;
}

static void submit_bulk_in(void)
{
    if (!s_dev || !s_bulk_in_xfer) return;
    if (s_bulk_in_pending) return;
    s_bulk_in_done = false;
    s_bulk_in_pending = true;
    s_bulk_in_xfer->num_bytes = RNDIS_MAX_TRANSFER_SIZE;
    s_bulk_in_xfer->device_handle = s_dev;
    s_bulk_in_xfer->bEndpointAddress = s_bulk_in_ep;
    s_bulk_in_xfer->callback = bulk_in_xfer_cb;
    s_bulk_in_xfer->context = NULL;
    esp_err_t err = usb_host_transfer_submit(s_bulk_in_xfer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Bulk IN submit err: %s", esp_err_to_name(err));
        s_bulk_in_pending = false;
    }
}

void usb_rndis_host_task(void)
{
    uint32_t evt_flags = 0;
    usb_host_lib_handle_events(pdMS_TO_TICKS(1), &evt_flags);
    if (s_client) {
        usb_host_client_handle_events(s_client, pdMS_TO_TICKS(1));
    }

    if (s_device_gone) {
        s_device_gone = false;
        ESP_LOGI(TAG, "USB device removed");
        close_device();
        s_state = RNDIS_STATE_DISCONNECTED;
        return;
    }

    switch (s_state) {
        case RNDIS_STATE_DISCONNECTED:
            if (s_has_new_device) {
                s_has_new_device = false;
                ESP_LOGI(TAG, "New USB device, addr=%d", s_pending_addr);
                s_state = RNDIS_STATE_DEV_DETECTED;
            }
            break;

        case RNDIS_STATE_DEV_DETECTED:
            if (open_device()) {
                s_state = RNDIS_STATE_CLAIMED;
                ESP_LOGI(TAG, "Device opened VID=%04X PID=%04X", s_vid, s_pid);
            } else {
                ESP_LOGW(TAG, "Open device failed");
                close_device();
                s_state = RNDIS_STATE_DISCONNECTED;
            }
            break;

        case RNDIS_STATE_CLAIMED:
            if (rndis_initialize()) {
                s_state = RNDIS_STATE_READY;
                ESP_LOGI(TAG, "RNDIS ready, MAC=%02X:%02X:%02X:%02X:%02X:%02X",
                         s_dev_mac[0], s_dev_mac[1], s_dev_mac[2],
                         s_dev_mac[3], s_dev_mac[4], s_dev_mac[5]);
            } else {
                ESP_LOGE(TAG, "RNDIS init failed");
                close_device();
                s_state = RNDIS_STATE_ERROR;
            }
            break;

        case RNDIS_STATE_READY:
            if (s_bulk_in_done) {
                s_bulk_in_done = false;
                s_last_rx_len = s_bulk_in_xfer->actual_num_bytes;
                s_rx_available = true;
            }
            if (!s_rx_available) {
                submit_bulk_in();
            }
            break;

        case RNDIS_STATE_ERROR:
            vTaskDelay(pdMS_TO_TICKS(1000));
            close_device();
            s_state = RNDIS_STATE_DISCONNECTED;
            break;
    }
}

bool usb_rndis_host_is_ready(void) { return s_state == RNDIS_STATE_READY; }
bool usb_rndis_host_is_connected(void) { return s_state != RNDIS_STATE_DISCONNECTED && s_state != RNDIS_STATE_ERROR; }

const char *usb_rndis_host_state_str(void)
{
    switch (s_state) {
        case RNDIS_STATE_DISCONNECTED: return "DISCONNECTED";
        case RNDIS_STATE_DEV_DETECTED: return "DEV_DETECTED";
        case RNDIS_STATE_CLAIMED: return "CLAIMED";
        case RNDIS_STATE_READY: return "READY";
        case RNDIS_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

int usb_rndis_host_send_packet(const uint8_t *buffer, size_t len)
{
    if (s_state != RNDIS_STATE_READY || !s_dev || !s_bulk_out_xfer) return -1;
    if (len > 1514) return -2;

    uint32_t msg_len = sizeof(rndis_packet_msg_t) + len;
    uint32_t pad = (4 - (msg_len & 3)) & 3;
    msg_len += pad;
    if (msg_len > RNDIS_MAX_TRANSFER_SIZE) return -3;

    uint8_t *buf = s_bulk_out_xfer->data_buffer;
    memset(buf, 0, msg_len);
    rndis_packet_msg_t *pkt = (rndis_packet_msg_t *)buf;
    pkt->MessageType = RNDIS_MSG_PACKET;
    pkt->MessageLength = msg_len;
    pkt->DataOffset = sizeof(rndis_packet_msg_t) - 8;
    pkt->DataLength = len;
    memcpy(buf + sizeof(rndis_packet_msg_t), buffer, len);

    s_bulk_out_xfer->num_bytes = msg_len;
    s_bulk_out_xfer->device_handle = s_dev;
    s_bulk_out_xfer->bEndpointAddress = s_bulk_out_ep;
    s_bulk_out_xfer->callback = bulk_out_xfer_cb;
    s_bulk_out_xfer->context = NULL;

    s_bulk_out_done = false;
    esp_err_t err = usb_host_transfer_submit(s_bulk_out_xfer);
    if (err != ESP_OK) return -4;

    TickType_t start = xTaskGetTickCount();
    while (!s_bulk_out_done && (xTaskGetTickCount() - start < pdMS_TO_TICKS(3000))) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), &flags);
        usb_host_client_handle_events(s_client, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (!s_bulk_out_done) return -5;
    return (int)len;
}

int usb_rndis_host_receive_packet(uint8_t *buffer, size_t max_len)
{
    if (s_state != RNDIS_STATE_READY || !s_rx_available) return 0;

    uint8_t *data = s_bulk_in_xfer->data_buffer;
    size_t actual = s_bulk_in_xfer->actual_num_bytes;

    if (actual < sizeof(rndis_packet_msg_t)) {
        s_rx_available = false;
        return 0;
    }

    rndis_packet_msg_t *pkt = (rndis_packet_msg_t *)data;
    if (pkt->MessageType != RNDIS_MSG_PACKET) {
        s_rx_available = false;
        return 0;
    }

    uint8_t *eth_frame = (uint8_t *)&pkt->DataOffset + pkt->DataOffset;
    uint32_t eth_len = pkt->DataLength;
    if (eth_len > max_len) eth_len = max_len;
    if (eth_len > actual) eth_len = (uint32_t)actual;
    memcpy(buffer, eth_frame, eth_len);

    s_rx_available = false;
    return (int)eth_len;
}

const uint8_t *usb_rndis_host_get_mac(void) { return s_dev_mac; }

uint16_t usb_rndis_host_get_vid(void) { return s_vid; }
uint16_t usb_rndis_host_get_pid(void) { return s_pid; }
