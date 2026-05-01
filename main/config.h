#ifndef CONFIG_APP_H
#define CONFIG_APP_H

#define PROJECT_NAME        "ESP32-S3-RNDIS-Bridge"
#define FIRMWARE_VERSION    "2.0.2"

#define WEB_SERVER_PORT     80

#define DEFAULT_AP_SSID     "ESP32-S3-RNDIS"
#define DEFAULT_AP_PASSWORD "12345678"
#define AP_CHANNEL          6
#define AP_MAX_STA          4

#define RNDIS_MAX_TRANSFER_SIZE 1536

#define NVS_NAMESPACE       "rndis-bridge"
#define NVS_KEY_STA_SSID    "sta_ssid"
#define NVS_KEY_STA_PASS    "sta_pass"

typedef struct {
    uint16_t vid;
    uint16_t pid;
    const char *name;
} rndis_device_desc_t;

static const rndis_device_desc_t RNDIS_DEVICE_TABLE[] = {
    {0x04e8, 0x6863, "Samsung RNDIS"},
    {0x04e8, 0x6860, "Samsung Modem+RNDIS"},
    {0x12d1, 0x1039, "Huawei RNDIS"},
    {0x12d1, 0x15ca, "Huawei RNDIS"},
    {0x19d2, 0x1405, "ZTE RNDIS"},
    {0x2717, 0xff40, "Xiaomi RNDIS"},
    {0x2a70, 0xff00, "OnePlus RNDIS"},
    {0x05c6, 0x9024, "Qualcomm RNDIS"},
    {0x2c7c, 0x0125, "Quectel EC25"},
    {0x2c7c, 0x0121, "Quectel EC21"},
    {0x1e0e, 0x9001, "SIMCom RNDIS"},
    {0x0e8d, 0x2004, "MediaTek RNDIS"},
    {0x0e8d, 0x2000, "MediaTek RNDIS"},
    {0x19a5, 0x0402, "Harris Remote NDIS"},
};
#define RNDIS_DEVICE_COUNT (sizeof(RNDIS_DEVICE_TABLE) / sizeof(RNDIS_DEVICE_TABLE[0]))

#endif
