#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct {char profile_message[96],ap[33],code[17],ip[16],ssid[33],call[8],network[96],identity[80],relay[80];bool online,mqtt_online,ap_active;int wifi_reason,wifi_rssi;} device_view_t;
esp_err_t device_web_start(void);
void device_web_view(device_view_t *v);

bool device_select_relay(int index);

void device_management_ap(bool enabled);

#define DEVICE_PROFILE_MAX 10
typedef struct {int id;char name[97];bool current,has_certificate;} device_profile_t;
int device_profiles(device_profile_t out[DEVICE_PROFILE_MAX]);
bool device_profile_switch(int id);
