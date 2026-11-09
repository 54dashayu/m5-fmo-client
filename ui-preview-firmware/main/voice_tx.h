#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
// Audio-task-owned FIFO. Never waits for a transport lock or replays across sessions.
#define VOICE_TX_BYTES 752
#define VOICE_TX_SLOTS 8
#define VOICE_TX_MAX_AGE_US 640000
 typedef struct {size_t n;int64_t at;uint8_t data[VOICE_TX_BYTES];} voice_tx_item_t;
typedef struct {voice_tx_item_t items[VOICE_TX_SLOTS];unsigned head,count;uint32_t sent,retries,dropped;} voice_tx_t;
static inline void voice_tx_pop(voice_tx_t*q){q->head=(q->head+1)%VOICE_TX_SLOTS;q->count--;}
static inline void voice_tx_clear(voice_tx_t*q){q->dropped+=q->count;q->count=0;q->head=0;}
static inline void voice_tx_expire(voice_tx_t*q,int64_t now){while(q->count&&now-q->items[q->head].at>=VOICE_TX_MAX_AGE_US){voice_tx_pop(q);q->dropped++;}}
static inline bool voice_tx_push(voice_tx_t*q,const uint8_t*p,size_t n,int64_t now){
 if(!n||n>VOICE_TX_BYTES)return false;
 voice_tx_expire(q,now);
 if(q->count==VOICE_TX_SLOTS){voice_tx_pop(q);q->dropped++;}
 voice_tx_item_t*i=&q->items[(q->head+q->count)%VOICE_TX_SLOTS];i->n=n;i->at=now;memcpy(i->data,p,n);q->count++;return true;
}
static inline void voice_tx_pump(voice_tx_t*q,int64_t now,bool(*send)(const uint8_t*,size_t)){
 voice_tx_expire(q,now);
 // Bounded catch-up, preserving order. Failed enqueue retains the exact packet.
 for(int budget=2;q->count&&budget>0;budget--){voice_tx_item_t*i=&q->items[q->head];if(!send(i->data,i->n)){q->retries++;break;}voice_tx_pop(q);q->sent++;}
}
