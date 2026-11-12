#pragma once
#include "relays.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#define DISCOVERY_MAX 8
typedef bool (*discovery_identity_fn)(const relay_t*,char**,char[8],uint32_t*,char[65]);
esp_err_t discovery_start(discovery_identity_fn identity,SemaphoreHandle_t allocation_lock);
void discovery_refresh(void);
void discovery_identity_changed(void);
int discovery_list(relay_t out[DISCOVERY_MAX],uint32_t *revision);
const char *discovery_select(int index,uint32_t revision);
cJSON *discovery_json(void);

void discovery_tick(void);

void discovery_status(char *out,size_t size);

const char *discovery_add(int index,uint32_t revision);
