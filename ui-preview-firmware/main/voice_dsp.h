#pragma once
#include <stdint.h>
#include <stdbool.h>
// ADC input is biased. Seed the DC estimator from the first real sample to
// avoid transmitting a large startup pulse; 80 Hz high-pass removes low hum.
typedef struct {int32_t dc;bool seeded;} voice_mic_filter_t;
static inline int16_t voice_mic_sample(voice_mic_filter_t*f,unsigned raw){
 int x=raw&4095;if(!f->seeded){f->dc=x*256;f->seeded=true;}
 f->dc+=(x*256-f->dc)/16;
 int s=(x-f->dc/256)*12,sign=s<0?-1:1,m=s<0?-s:s;
 // Gentle knee instead of hard clipping close speech.
 if(m>24000)m=24000+(m-24000)/4;
 if(m>32700)m=32700;
 return (int16_t)(sign*m);
}
// Preserve sub-LSB signal at low volume with first-order error feedback.
// Slew gain and bridge signal/silence transitions over 4 ms to avoid clicks.
typedef struct {int gain,error,last;unsigned ramp;bool silent;} voice_dac_filter_t;
static inline uint16_t voice_dac_sample(voice_dac_filter_t*f,int pcm,int volume,bool silence){
 int target=volume*256;if(f->gain<target){f->gain+=64;if(f->gain>target)f->gain=target;}
 if(f->gain>target){f->gain-=64;if(f->gain<target)f->gain=target;}
 if(silence!=f->silent){f->silent=silence;f->ramp=32;}
 int s=pcm*f->gain/256/100;
 if(f->ramp){s=f->last+(s-f->last)/(int)f->ramp;f->ramp--;}
 f->last=s;int value=s+f->error;int q=(value+32768+128)/256-128;
 if(q>127)q=127;
 if(q< -128)q=-128;
 f->error=value-q*256;
 if(f->error>255)f->error=255;
 if(f->error< -255)f->error=-255;
 return (uint16_t)(q+128)<<8;
}
