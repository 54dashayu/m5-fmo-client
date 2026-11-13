#pragma once
#include "sstv_decoder.h"
#include <stddef.h>
#define SSTV_GALLERY_MAX 20
typedef struct { uint32_t id, saved_at; unsigned width,height; size_t file_bytes; bool compressed,partial; unsigned received_lines; char relay[128]; } sstv_entry_t;
/* Host tests pass a temporary directory; firmware passes NULL. */
const char *sstv_store_init(const char *path);
const char *sstv_store_error(void);
int sstv_store_list(sstv_entry_t *out);
bool sstv_store_save(const uint8_t *pixels, uint32_t saved_at, uint32_t *id);
bool sstv_store_load(uint32_t id, uint8_t *pixels);
bool sstv_store_delete(uint32_t id);

bool sstv_store_save_relay(const uint8_t *pixels,uint32_t at,uint32_t *id,const char *relay);

typedef struct { size_t total,used,safe_limit; unsigned count,remaining; bool low; } sstv_storage_t;
bool sstv_store_stats(sstv_storage_t *out);
const char *sstv_store_save_error(void);

#ifdef FMO_SSTV_STREAMING
bool sstv_stream_prepare(void);
bool sstv_stream_begin(const char *relay);
bool sstv_stream_rows(unsigned first,const uint8_t *pixels);
bool sstv_stream_finish(uint32_t at);
bool sstv_stream_save(uint32_t *id);
void sstv_stream_discard(void);
bool sstv_store_preview(uint32_t id,uint8_t *indexed);
typedef bool (*sstv_stream_writer_t)(void *,const uint8_t *,size_t);
bool sstv_store_export(uint32_t id,sstv_stream_writer_t writer,void *ctx);
#endif

bool sstv_store_save_partial(const uint8_t *pixels,uint32_t at,uint32_t *id,const char *relay,unsigned lines);
#ifdef FMO_SSTV_STREAMING
bool sstv_stream_finish_partial(uint32_t at);
#endif

bool sstv_store_needs_initialization(void);
const char *sstv_store_initialize_confirmed(void);
