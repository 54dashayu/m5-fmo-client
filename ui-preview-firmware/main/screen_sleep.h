#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {bool sleeping;uint8_t consume;} screen_sleep_t;
static inline void screen_sleep_enter(screen_sleep_t*s){s->sleeping=true;s->consume=0;}
// A waking press is consumed until release; it must not transmit or change volume.
static inline uint8_t screen_sleep_step(screen_sleep_t*s,bool call_active,uint8_t pressed){
 if(s->sleeping&&(call_active||pressed)){s->sleeping=false;s->consume=pressed;}
 if(s->consume){if(!pressed)s->consume=0;return 0;}
 return s->sleeping?0:pressed;
}
