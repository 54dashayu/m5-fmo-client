#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"
typedef struct { bool valid; float cpu[2], average; uint32_t window_ms, internal_total, internal_free, internal_largest, psram_total, psram_free; } resource_metrics_t;
#if defined(CONFIG_FMO_BOARD_XINGZHI) || defined(CONFIG_IDF_TARGET_ESP32)
void resource_metrics_sample(void);
void resource_metrics_read(resource_metrics_t *out);
#else
static inline void resource_metrics_sample(void) {}
#endif
