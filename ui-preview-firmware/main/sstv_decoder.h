#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(CONFIG_FMO_BOARD_XINGZHI) || defined(CONFIG_IDF_TARGET_ESP32)
#define FMO_FULL_SSTV 1
#define SSTV_WIDTH 320
#define SSTV_HEIGHT 240
#define SSTV_BPP 2
#else
#define SSTV_WIDTH 160
#define SSTV_HEIGHT 120
#define SSTV_BPP 1
#endif
#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(SSTV_HOST_FULL_FRAME)
#define FMO_SSTV_STREAMING 1
#define SSTV_PREVIEW_WIDTH 120
#define SSTV_PREVIEW_HEIGHT 90
#define SSTV_PREVIEW_BYTES (SSTV_PREVIEW_WIDTH*SSTV_PREVIEW_HEIGHT)
#endif
#define SSTV_PIXELS (SSTV_WIDTH*SSTV_HEIGHT)
#define SSTV_BYTES (SSTV_PIXELS*SSTV_BPP)
typedef enum { SSTV_WAITING, SSTV_RECEIVING, SSTV_COMPLETE, SSTV_INTERRUPTED } sstv_status_t;
/* Portable 8 kHz Robot36 receiver. RGB332 preview, no runtime allocations. */
typedef struct {
    #ifdef FMO_SSTV_STREAMING
    uint8_t image[320*2*2];
    bool (*begin)(void *ctx);
    bool (*rows)(void *ctx,unsigned first,const uint8_t *pixels);
    void *sink;
#else
    uint8_t image[SSTV_BYTES];
#endif
    uint8_t even_y[SSTV_WIDTH], even_cr[SSTV_WIDTH];
    float fir[31], taps[31], prev_i, prev_q, smooth, line[1140];
    unsigned leader_max,vis_starts,leader_gap;
    unsigned pos, clock, leader, sync_run, vis_start, vis_bit, vis_code;
    unsigned line_start, lines, generation, header_deadline;
    float vis_sum; unsigned vis_count;
    bool armed, vis, scanning, pair;
    sstv_status_t status;
} sstv_decoder_t;
void sstv_decoder_init(sstv_decoder_t *d);
/* Preserve the last preview; require a fresh valid VIS after any discontinuity. */
void sstv_decoder_break(sstv_decoder_t *d);
void sstv_decoder_feed(sstv_decoder_t *d, const int16_t *pcm, size_t count);
