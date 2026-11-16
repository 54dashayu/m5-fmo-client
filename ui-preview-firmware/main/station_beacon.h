#pragma once
#include "station_settings.h"
// sha256 callback keeps the serializer independently testable on host and ESP32.
typedef bool (*beacon_hash_fn)(const uint8_t*,size_t,uint8_t[32]);
size_t station_beacon(char out[513],const station_settings_t*s,const cJSON*user_cert,const uint8_t secret[64],int64_t now,beacon_hash_fn hash);
