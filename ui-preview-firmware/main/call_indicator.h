#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t color;uint8_t opacity;} call_indicator_t;
static inline bool call_flashing_enabled(bool has_leds,bool breathing){return !has_leds||!breathing;}
// Keep direction colors even with flashing disabled. TX takes precedence.
static inline call_indicator_t call_indicator(bool tx,bool rx,bool flash,uint64_t ms){
 call_indicator_t v={.color=tx?0xff3030u:rx?0x20ff70u:0xffd166u,.opacity=255};
 if(flash&&(tx||rx)&&(ms%800)>=400)v.opacity=0;
 return v;
}
