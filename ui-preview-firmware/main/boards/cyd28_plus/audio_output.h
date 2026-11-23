#pragma once
#include <stdint.h>
// 500Hz at 8kHz, modest amplitude, 20ms cosine-free linear envelope.
static inline int16_t cyd_test_tone(unsigned sample,unsigned total){
 static const int16_t wave[16]={0,689,1273,1663,1800,1663,1273,689,0,-689,-1273,-1663,-1800,-1663,-1273,-689};
 if(sample>=total)return 0;
 unsigned gain=160;if(sample<gain)gain=sample;
 if(total-1-sample<gain)gain=total-1-sample;
 return wave[sample%16]*(int)gain/160;
}

// Minimal receive path: signed PCM16 -> unsigned DAC8, no error-feedback dither.
static inline uint8_t cyd_dac_byte(int16_t pcm,int volume){
 if(volume<0)volume=0;
 if(volume>100)volume=100;
 int32_t value=(int32_t)pcm*volume/100;
 return (uint8_t)((value+32768)/256);
}
