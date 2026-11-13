#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define VOICE_WIRE_PACKET_MAX 2048
// A frame requires a 16-byte header and at least one payload byte.
#define VOICE_WIRE_FRAMES_MAX ((VOICE_WIRE_PACKET_MAX-64)/17)
typedef struct {uint32_t uid,server,base,timestamp;char call[13];int count;const uint8_t*raw[VOICE_WIRE_FRAMES_MAX];uint16_t len[VOICE_WIRE_FRAMES_MAX];uint8_t mode[VOICE_WIRE_FRAMES_MAX];} voice_packet_t;
bool voice_parse(const uint8_t*p,size_t n,voice_packet_t*out);
void radpcm_encode(const int16_t*pcm,uint8_t raw[328],uint16_t index,int*predictor,int*step);
bool radpcm_decode(const uint8_t raw[328],int16_t pcm[640]);
size_t voice_pack(uint8_t*out,size_t cap,const uint8_t*frames,int count,uint32_t uid,const char*call,uint32_t server,uint32_t base,uint32_t timestamp,uint16_t first);
