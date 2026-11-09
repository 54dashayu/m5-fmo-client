#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef enum {KEY_SHORT,KEY_LONG,KEY_PTT_START,KEY_PTT_STOP,KEY_CANCEL} key_action_t;
typedef void (*key_callback_t)(int,key_action_t,void*);
typedef struct {uint8_t raw,stable;uint32_t changed[3],down[3],last_b;bool held[3],ptt,blocked,armed;key_callback_t callback;void*context;} buttons_t;
void buttons_init(buttons_t*b,uint8_t pressed,key_callback_t cb,void*ctx);
void buttons_step(buttons_t*b,uint8_t pressed,uint32_t now,bool home);
void buttons_block(buttons_t*b);
