#include <assert.h>
#include <stdlib.h>
#include "voice_dsp.h"
int main(void){
 voice_mic_filter_t mic={0};
 // Biased ADC silence must not become a loud startup pulse.
 for(int i=0;i<640;i++)assert(voice_mic_sample(&mic,2800)==0);
 for(int i=0;i<6400;i++){int x=(i%8<4)?4095:0;int s=voice_mic_sample(&mic,x);assert(s<=32700&&s>=-32700);}
 for(int i=0;i<6400;i++)voice_mic_sample(&mic,2800);
 assert(abs(voice_mic_sample(&mic,2800))<24);
 voice_dac_filter_t dac={0};long total=0;
 // A sub-8-bit-LSB signal remains audible on average at low volume.
 for(int i=0;i<8000;i++){int s=(voice_dac_sample(&dac,400,25,false)>>8)-128;if(i>=4000)total+=s;}
 assert(total>1400&&total<1700);
 for(int i=0;i<1000;i++)voice_dac_sample(&dac,0,25,true);
 assert(voice_dac_sample(&dac,0,25,true)==0x8000);
 for(int i=0;i<1000;i++)voice_dac_sample(&dac,32767,0,false);
 assert(voice_dac_sample(&dac,32767,0,false)==0x8000);
 return 0;
}
