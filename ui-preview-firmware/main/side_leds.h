#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {bool ready;unsigned writes,failures;int color,brightness,error,route,open_drain;} side_leds_view_t;
void side_leds_init(void);
void side_leds_update(bool tx,bool rx,int64_t now_ms);
void side_leds_view(side_leds_view_t*out);
