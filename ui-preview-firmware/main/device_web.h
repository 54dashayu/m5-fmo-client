#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct {char ap[33],code[17],ip[16],ssid[33],call[8],network[96],identity[80],relay[80];bool online,mqtt_online,ap_active;int wifi_reason,wifi_rssi;} device_view_t;
esp_err_t device_web_start(void);
void device_web_view(device_view_t *v);

bool device_select_relay(int index);

void device_management_ap(bool enabled);
