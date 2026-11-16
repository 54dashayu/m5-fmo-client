#pragma once
#include "sstv_decoder.h"
#include <stdatomic.h>
#ifdef FMO_SSTV_STREAMING
/* Gallery and receiver never run together. Ownership also protects USB prepare calls. */
typedef union {
 struct {uint8_t pixels[80*60],sides[2][40*30];} gallery;
 uint8_t tx_rows[11][640];
 struct {sstv_decoder_t decoder;uint8_t preview[SSTV_PREVIEW_BYTES];} rx;
} sstv_m5_workspace_t;
extern sstv_m5_workspace_t sstv_m5_workspace;
extern atomic_int sstv_m5_workspace_owner;
enum {SSTV_WORKSPACE_GALLERY=1,SSTV_WORKSPACE_RX=2,SSTV_WORKSPACE_TX=3};
static inline bool sstv_workspace_claim(int owner){int expected=0;return atomic_compare_exchange_strong(&sstv_m5_workspace_owner,&expected,owner)||expected==owner;}
static inline void sstv_workspace_release(int owner){atomic_compare_exchange_strong(&sstv_m5_workspace_owner,&owner,0);}
#endif
