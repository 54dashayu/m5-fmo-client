#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Timestamp granularity differs between senders. Only reject a repeated
 * header when the already-validated payload CRC and length also match. */
typedef struct {uint32_t uid,base,stamp,crc;uint16_t bytes;bool seen;} voice_dedup_t;
static inline bool voice_duplicate(voice_dedup_t *s,uint32_t uid,uint32_t base,uint32_t stamp,uint32_t crc,uint16_t bytes,bool *shared_stamp){
 bool header=s->seen&&s->uid==uid&&s->base==base&&s->stamp==stamp;
 bool duplicate=header&&s->crc==crc&&s->bytes==bytes;
 *shared_stamp=header&&!duplicate;
 *s=(voice_dedup_t){uid,base,stamp,crc,bytes,true};
 return duplicate;
}
