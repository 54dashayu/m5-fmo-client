#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define ROGER_SAMPLES 2560
// Two tones: 500 Hz for 140 ms, 40 ms silence, 1 kHz for 140 ms.
// 8 kHz PCM, 10 ms envelopes; peak 4000 avoids clipping.
static inline int16_t roger_sample(unsigned n){
 static const int16_t wave[16]={0,1531,2828,3696,4000,3696,2828,1531,0,-1531,-2828,-3696,-4000,-3696,-2828,-1531};
 if(n>=ROGER_SAMPLES||(n>=1120&&n<1440))return 0;
 unsigned t=n<1120?n:n-1440,step=n<1120?1:2;
 unsigned gain=t<80?t:1119-t<80?1119-t:80;
 return (int32_t)wave[(t*step)&15]*(int32_t)gain/80;
}
static inline bool roger_allowed(bool connected,bool same_session,bool receiving,bool testing,const char*reason,unsigned captured,unsigned dropped,unsigned queued,unsigned capacity){return connected&&same_session&&!receiving&&!testing&&!strcmp(reason,"key_or_page")&&captured>0&&dropped==0&&queued<capacity;}
