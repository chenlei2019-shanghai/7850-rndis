#ifndef USB_RNDIS_HOST_H
#define USB_RNDIS_HOST_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RNDIS_STATE_DISCONNECTED = 0,
    RNDIS_STATE_DEV_DETECTED,
    RNDIS_STATE_CLAIMED,
    RNDIS_STATE_READY,
    RNDIS_STATE_ERROR
} rndis_state_t;

esp_err_t usb_rndis_host_init(void);
void usb_rndis_host_task(void);
bool usb_rndis_host_is_ready(void);
bool usb_rndis_host_is_connected(void);
const char *usb_rndis_host_state_str(void);
int usb_rndis_host_send_packet(const uint8_t *buffer, size_t len);
int usb_rndis_host_receive_packet(uint8_t *buffer, size_t max_len);
const uint8_t *usb_rndis_host_get_mac(void);
uint16_t usb_rndis_host_get_vid(void);
uint16_t usb_rndis_host_get_pid(void);

#endif
