#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "voice_wire.h"
// Store only received bytes: short Opus packets must not occupy maximum-size slots.
// Fixed allocation remains below the former four full packet slots.
#define VOICE_RX_BYTES 5120
#define VOICE_RX_SLOTS 24
#define VOICE_RX_PACKET_MAX VOICE_WIRE_PACKET_MAX
typedef struct {uint16_t offset,n,samples;int64_t arrival;} voice_rx_entry_t;
typedef struct {uint8_t bytes[VOICE_RX_BYTES];voice_rx_entry_t entries[VOICE_RX_SLOTS];unsigned head,count,write,used,samples;} voice_rx_t;
static inline void voice_rx_clear(voice_rx_t*q){q->head=q->count=q->write=q->used=q->samples=0;}
static inline void voice_rx_drop(voice_rx_t*q){if(!q->count)return;voice_rx_entry_t*e=&q->entries[q->head];q->used-=e->n;q->samples-=e->samples;q->head=(q->head+1)%VOICE_RX_SLOTS;q->count--;}
// Caller serializes access. Returns number of oldest packets removed, -1 invalid.
static inline int voice_rx_push(voice_rx_t*q,const uint8_t*p,unsigned n,unsigned samples,int64_t arrival){
 if(!p||!n||n>VOICE_RX_PACKET_MAX||samples>65535)return -1;
 int dropped=0;while(q->count==VOICE_RX_SLOTS||q->used+n>VOICE_RX_BYTES){voice_rx_drop(q);dropped++;}
 voice_rx_entry_t*e=&q->entries[(q->head+q->count)%VOICE_RX_SLOTS];*e=(voice_rx_entry_t){.offset=q->write,.n=n,.samples=samples,.arrival=arrival};
 unsigned first=VOICE_RX_BYTES-q->write;if(first>n)first=n;memcpy(q->bytes+q->write,p,first);memcpy(q->bytes,p+first,n-first);
 q->write=(q->write+n)%VOICE_RX_BYTES;q->used+=n;q->samples+=samples;q->count++;return dropped;
}
static inline bool voice_rx_pop(voice_rx_t*q,uint8_t*out,uint16_t*n,int64_t*arrival){
 if(!q->count)return false;
 voice_rx_entry_t e=q->entries[q->head];unsigned first=VOICE_RX_BYTES-e.offset;if(first>e.n)first=e.n;
 memcpy(out,q->bytes+e.offset,first);memcpy(out+first,q->bytes,e.n-first);*n=e.n;*arrival=e.arrival;voice_rx_drop(q);return true;
}
// Wait for media duration, not a fixed packet count. Release early before a
// full-size next packet would overflow, and bound the wait by the chosen window.
static inline bool voice_rx_prebuffer_for(const voice_rx_t*q,int64_t elapsed,unsigned ms){return q->count&&elapsed<(int64_t)ms*1000&&q->samples<ms*8&&q->count<VOICE_RX_SLOTS-1&&VOICE_RX_BYTES-q->used>=VOICE_RX_PACKET_MAX;}

static inline bool voice_rx_prebuffer(const voice_rx_t*q,int64_t elapsed){return voice_rx_prebuffer_for(q,elapsed,240);}

// One aggregated packet is not a jitter cushion: it can contain all the media
// until the next network delivery. Wait for a second packet or the deadline.
static inline bool voice_rx_jitter_prebuffer(const voice_rx_t*q,int64_t elapsed,unsigned ms){
 return q->count&&elapsed<(int64_t)ms*1000&&(q->count<2||q->samples<ms*8)&&q->count<VOICE_RX_SLOTS-1&&VOICE_RX_BYTES-q->used>=VOICE_RX_PACKET_MAX;
}
#ifdef CONFIG_FMO_BOARD_XINGZHI
#define voice_rx_playback_prebuffer voice_rx_prebuffer_for
#define VOICE_RX_STALE_US 1000000
#else
#define voice_rx_playback_prebuffer voice_rx_jitter_prebuffer
#define VOICE_RX_STALE_US 2000000
#endif
