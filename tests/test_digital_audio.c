#include "digital_audio_dsp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double energy(double hz){
 digital_fir_t f={0};double sum=0;int32_t in[320];int16_t out[160];
 for(int block=0;block<30;block++){
  for(int i=0;i<320;i++)in[i]=(int32_t)(10000*sin(2*3.141592653589793*hz*(block*320+i)/16000))*65536;
  digital_down2(&f,in,out,160);
  if(block>0)for(int i=0;i<160;i++)sum+=(double)out[i]*out[i];
 }
 return sum;
}
int main(void){
 digital_fir_t a={0},b={0};int16_t in[160];int32_t full[480],split[480];
 for(int i=0;i<160;i++)in[i]=(i%17)*1000-8000;
 digital_up3(&a,in,full,160,100);digital_up3(&b,in,split,71,100);digital_up3(&b,in+71,split+213,89,100);
 assert(!memcmp(full,split,sizeof(full)));
 digital_up3(&a,in,full,160,0);for(int i=0;i<480;i++)assert(full[i]==0);
 memset(&a,0,sizeof(a));for(int i=0;i<160;i++)in[i]=10000;
 digital_up3(&a,in,full,160,100);double mean=0;for(int i=240;i<480;i++)mean+=full[i]/65536.0;mean/=240;
 assert(mean>9950&&mean<10050);
 double pass=energy(1000),alias=energy(6000);assert(pass>1e10);assert(alias/pass<0.001);
 assert(digital_clip(INT64_MAX)==32767&&digital_clip(INT64_MIN)==-32768);
 printf("PASS digital resampling: continuous blocks, mute, DC gain, alias rejection %.1f dB, saturation\n",10*log10(alias/pass));
}
