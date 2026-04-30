#ifndef NET_BRIDGE_H
#define NET_BRIDGE_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t net_bridge_init(void);
void net_bridge_task(void);
void net_bridge_enable_nat(bool on);

#endif
