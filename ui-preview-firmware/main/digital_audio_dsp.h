#pragma once
#include <stdint.h>
#include <string.h>
#define DIGITAL_TAPS 47
typedef struct {int16_t h[DIGITAL_TAPS];unsigned pos;} digital_fir_t;
static const int16_t digital_rx_coeff[DIGITAL_TAPS]={-24,-36,11,62,19,-95,-90,105,211,-45,-358,-137,463,468,-421,-934,94,1469,717,-1972,-2547,2331,10116,13954,10116,2331,-2547,-1972,717,1469,94,-934,-421,468,463,-137,-358,-45,211,105,-90,-95,19,62,11,-36,-24};
static const int16_t digital_tx_coeff[DIGITAL_TAPS]={36,27,-8,-53,-75,-33,75,178,161,-30,-300,-420,-194,330,794,727,-63,-1200,-1805,-988,1517,5009,8062,9274,8062,5009,1517,-988,-1805,-1200,-63,727,794,330,-194,-420,-300,-30,161,178,75,-33,-75,-53,-8,27,36};
static inline int16_t digital_clip(int64_t x){return x>32767?32767:x<-32768?-32768:(int16_t)x;}
static inline int16_t digital_filter(digital_fir_t *f,const int16_t *c,int16_t x,int gain){
 f->h[f->pos]=x;int64_t acc=0;unsigned p=f->pos;
 for(unsigned i=0;i<DIGITAL_TAPS;i++){acc+=(int32_t)c[i]*f->h[p];p=p?p-1:DIGITAL_TAPS-1;}
 f->pos=(f->pos+1)%DIGITAL_TAPS;return digital_clip(acc*gain/32768);
}
static inline void digital_down2(digital_fir_t *f,const int32_t *in,int16_t *out,unsigned n){
 for(unsigned i=0;i<n;i++){digital_filter(f,digital_rx_coeff,(int16_t)(in[2*i]/65536),1);out[i]=digital_filter(f,digital_rx_coeff,(int16_t)(in[2*i+1]/65536),1);}
}
static inline void digital_up3(digital_fir_t *f,const int16_t *in,int32_t *out,unsigned n,int volume){
 if(volume<0)volume=0;
 if(volume>100)volume=100;
 for(unsigned i=0;i<n;i++)for(unsigned j=0;j<3;j++){
  int16_t v=digital_filter(f,digital_tx_coeff,j==0&&in?in[i]:0,3);
  out[3*i+j]=(int32_t)((int64_t)v*65536*volume*volume/10000);
 }
}
