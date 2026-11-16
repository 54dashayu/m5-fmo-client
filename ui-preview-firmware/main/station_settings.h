#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
typedef struct {
 uint32_t version;
 bool prefix,has_location;
 uint8_t aprs_ssid;
 double latitude,longitude,frequency,height;
 char rig[16],antenna[16],remark[64];
} station_settings_t;
void station_defaults(station_settings_t *s);
const char *station_parse(const cJSON *j,station_settings_t *out);
bool station_valid(const station_settings_t *s);
void station_json(cJSON *j,const station_settings_t *s);
void station_display_call(char out[13],const char *call,bool prefix);
