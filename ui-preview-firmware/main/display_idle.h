#pragma once
#include <stdbool.h>
#include <stdint.h>
#define DISPLAY_IDLE_MS 300000LL
typedef struct {int64_t last_activity; bool initialized;} display_idle_t;
static inline unsigned display_idle_step(display_idle_t *s,int64_t now,bool activity){
 if(!s->initialized||activity||now<s->last_activity){s->last_activity=now;s->initialized=true;}
 return now-s->last_activity>=DISPLAY_IDLE_MS?3:100;
}
