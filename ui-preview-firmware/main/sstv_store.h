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

typedef struct { uint64_t total,used,safe_limit; unsigned count,remaining; bool low; } sstv_storage_t;
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

/* Validated immutable read session. Close after cancellation/completion. */
typedef struct sstv_reader sstv_reader_t;
sstv_reader_t *sstv_store_reader_open(uint32_t id);
bool sstv_store_reader_row(void *reader,unsigned row,uint8_t *pixels);
void sstv_store_reader_close(sstv_reader_t *reader);

int sstv_store_page(sstv_entry_t *out,uint32_t before);
bool sstv_store_on_sd(void);

typedef bool (*sstv_import_read_t)(void *,uint8_t *,size_t);
bool sstv_store_import(sstv_import_read_t read,void *ctx,uint32_t at,uint32_t *id);

int sstv_store_newer(sstv_entry_t *out,uint32_t after);

const char *sstv_store_select(bool sd);
const char *sstv_store_card_manage(bool format);

unsigned sstv_store_revision(void);

/* Small RGB332 thumbnail, verified before display; streaming M5 only. */
bool sstv_store_thumbnail(uint32_t id,uint8_t *pixels,unsigned width,unsigned height);

/* Adjacent image ID, wrapping at album ends; zero for empty/busy storage. */
uint32_t sstv_store_neighbor(uint32_t id,bool newer);
