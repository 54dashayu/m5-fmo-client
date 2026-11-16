#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
typedef struct {char line[513],login[12],host[64];int port,passcode;} station_tx_packet_t;
typedef bool (*station_tx_prepare_fn)(station_tx_packet_t*,char reason[96]);
void station_tx_init(station_tx_prepare_fn prepare,bool(*busy)(void));
bool station_tx_update(bool enabled,bool immediate);
void station_tx_status(cJSON*j);
