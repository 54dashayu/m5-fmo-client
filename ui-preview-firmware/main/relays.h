#pragma once
#include "esp_err.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>
#define RELAY_PAGE_SIZE 8
typedef struct {char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;} relay_t;
esp_err_t relays_init(void);
int relays_count(void);
int relays_current(relay_t*out,uint32_t*revision);
bool relays_get(int index,relay_t*out);
bool relays_select(int index);
cJSON*relays_json(void);
const char*relays_add(const cJSON*j);

void relays_connection(const relay_t*target,bool success);
void relays_clear_evidence(void);

int relays_order_at(int position);
int relays_order_position(int index);
cJSON*relays_json_page(int offset);
bool relays_verified(const relay_t*target);
const char*relays_remove(int index,const relay_t*expected);

const char*relays_update_name(const cJSON*j);

void relays_use_temporary(const relay_t*relay);
