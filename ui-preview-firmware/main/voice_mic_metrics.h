#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
typedef struct {unsigned minimum,maximum,dc,raw_rms,pcm_rms,hum50_permille,hum100_permille,tag_errors,codec_error;} voice_mic_metrics_t;
static inline voice_mic_metrics_t voice_mic_measure(const uint16_t*raw,const int16_t*pcm,size_t n){
 voice_mic_metrics_t m={.minimum=4095};if(!n)return m;
 uint64_t sum=0,squares=0,energy=0;for(size_t i=0;i<n;i++){unsigned v=raw[i]&4095;sum+=v;squares+=(uint64_t)v*v;energy+=(int64_t)pcm[i]*pcm[i];if(v<m.minimum)m.minimum=v;if(v>m.maximum)m.maximum=v;if((raw[i]>>12)!=6)m.tag_errors++;}
 float mean=(float)sum/n,variance=(float)squares/n-mean*mean;if(variance<0)variance=0;m.dc=(unsigned)(mean+.5f);m.raw_rms=sqrtf(variance);m.pcm_rms=sqrtf((float)energy/n);
 float a1=0,a2=0,b1=0,b2=0;const float ca=1.9984580725f,cb=1.9938346675f;
 for(size_t i=0;i<n;i++){float x=(raw[i]&4095)-mean,a=x+ca*a1-a2,b=x+cb*b1-b2;a2=a1;a1=a;b2=b1;b1=b;}
 float denom=(float)n*n*variance;
 if(denom>0){float a=2000*(a1*a1+a2*a2-ca*a1*a2)/denom,b=2000*(b1*b1+b2*b2-cb*b1*b2)/denom;m.hum50_permille=a<0?0:a>1000?1000:(unsigned)a;m.hum100_permille=b<0?0:b>1000?1000:(unsigned)b;}
 return m;
}
static inline unsigned voice_mic_error(const int16_t*a,const int16_t*b,size_t n){uint64_t e=0;for(size_t i=0;i<n;i++){int64_t d=(int)a[i]-b[i];e+=d*d;}return n?(unsigned)sqrtf((float)e/n):0;}
