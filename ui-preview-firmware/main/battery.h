#pragma once
#include <stdbool.h>
#include <stdint.h>
void battery_init(void);
void battery_view(int *percent, bool *charging);
static inline int battery_decode(uint8_t value){switch(value&0xf0){case 0x00:return 100;case 0x80:return 75;case 0xc0:return 50;case 0xe0:return 25;case 0xf0:return 0;default:return -1;}}
