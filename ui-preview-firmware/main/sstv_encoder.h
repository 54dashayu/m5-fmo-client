#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Robot36, mono 8 kHz. Reader supplies one native RGB565 little-endian row. */
typedef bool (*sstv_row_reader_t)(void *,unsigned,uint8_t *);
typedef struct {
 sstv_row_reader_t read; void *ctx;
 uint32_t sample; float phase;
 unsigned cached_row; bool failed;
 uint8_t row[640];
} sstv_encoder_t;
// SSTV-only PCM peak: half the former 10000 amplitude (-6.02 dB).
// Applies uniformly to VIS, sync and image tones; does not affect voice or playback.
#define SSTV_TX_PCM_PEAK 5000
#define SSTV_TX_HEADER_SAMPLES 7280u
#define SSTV_TX_LINE_SAMPLES 1200u
#define SSTV_TX_TOTAL_SAMPLES (SSTV_TX_HEADER_SAMPLES+240u*SSTV_TX_LINE_SAMPLES+800u)
void sstv_encoder_init(sstv_encoder_t *,sstv_row_reader_t,void *);
size_t sstv_encoder_read(sstv_encoder_t *,int16_t *,size_t);
