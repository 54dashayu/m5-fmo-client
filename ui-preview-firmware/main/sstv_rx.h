#pragma once
#include "sstv_decoder.h"
bool sstv_rx_init(void);
/* Audio producer: bounded, never waits for the decoder or display. */
void sstv_rx_feed(const int16_t *pcm, size_t n, uint32_t uid, uint32_t base, uint32_t epoch);
void sstv_rx_break(void);
/* UI owns the returned indexed palette + pixels, updated only by snapshot. */
uint8_t *sstv_rx_pixels(void);
bool sstv_rx_snapshot(sstv_status_t *status, unsigned *lines, unsigned *generation);

bool sstv_rx_enable(bool enabled);
bool sstv_rx_save(uint32_t *id);

typedef struct {bool enabled,ready;unsigned init_error,internal_free,internal_largest,samples,stream_breaks,timeouts,drops,clipped,max_process_us,max_store_us,store_stalls,max_feed_gap_us,snapshots,snapshot_misses,preview_nonzero,lines,leader_max,vis_starts,vis_code;int frequency,status;} sstv_diagnostics_t;
bool sstv_rx_diagnostics(sstv_diagnostics_t *out);

void sstv_rx_relay(const char *relay);

bool sstv_rx_display_lock(void);
void sstv_rx_display_unlock(void);

/* Cheap opt-in query for M5 media buffering. */
bool sstv_rx_active(void);

bool sstv_rx_save_partial(uint32_t *id);
