#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {uint32_t uid,server,base,timestamp;char call[13];int count;const uint8_t*raw[6];uint16_t len[6];uint8_t mode[6];} voice_packet_t;
bool voice_parse(const uint8_t*p,size_t n,voice_packet_t*out);
void radpcm_encode(const int16_t*pcm,uint8_t raw[328],uint16_t index,int*predictor,int*step);
bool radpcm_decode(const uint8_t raw[328],int16_t pcm[640]);
size_t voice_pack(uint8_t*out,size_t cap,const uint8_t*frames,int count,uint32_t uid,const char*call,uint32_t server,uint32_t base,uint32_t timestamp,uint16_t first);
