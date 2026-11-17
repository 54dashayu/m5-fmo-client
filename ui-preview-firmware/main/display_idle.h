#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "screen_sleep.h"
#define DISPLAY_IDLE_MS 300000LL
#define DISPLAY_OFF_MS 600000LL
#define DISPLAY_IDLE_BRIGHTNESS_PERCENT 1
typedef struct {int64_t last_activity; bool initialized;} display_idle_t;
static inline unsigned display_idle_step(display_idle_t *s,int64_t now,bool activity){
 if(!s->initialized||activity||now<s->last_activity){s->last_activity=now;s->initialized=true;}
 int64_t elapsed=now-s->last_activity;
 return elapsed>=DISPLAY_OFF_MS?0:elapsed>=DISPLAY_IDLE_MS?DISPLAY_IDLE_BRIGHTNESS_PERCENT:100;
}

// Use the same wake-key consumption as manual screen sleep: waking must not PTT.
static inline unsigned display_idle_screen_step(display_idle_t *idle,screen_sleep_t *sleep,int64_t now,bool call_active,uint8_t pressed){
 unsigned brightness=display_idle_step(idle,now,call_active||pressed);
 if(!sleep->sleeping&&brightness==0)screen_sleep_enter(sleep);
 screen_sleep_step(sleep,call_active,pressed);
 return sleep->sleeping?0:brightness;
}
