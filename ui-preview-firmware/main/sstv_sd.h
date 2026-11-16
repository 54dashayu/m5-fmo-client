#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Boot-only mount; shares the already initialized LCD SPI bus. Never formats. */
const char *sstv_sd_mount(void);
bool sstv_sd_space(uint64_t *total,uint64_t *used);
bool sstv_sd_supported(void);
bool sstv_sd_ready(void);
const char *sstv_sd_error(void);
/* Caller must hold store lock and ensure no open image/stream sessions. */
bool sstv_sd_manage(bool format);
bool sstv_sd_pause_display(void);

/* Completed stages, not percentage or elapsed-time estimation. */
unsigned sstv_sd_manage_stage(void);
void sstv_sd_manage_reset(void);
