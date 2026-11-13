#include "voice_mic_metrics.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint16_t raw[640];int16_t pcm[640],other[640];for(int hz=50;hz<=1000;hz=hz==50?100:hz==100?1000:2000){for(int i=0;i<640;i++){int x=300*sinf(6.28318530718f*hz*i/8000);raw[i]=0x6000|(2048+x);pcm[i]=x*12;other[i]=pcm[i]+10;}voice_mic_metrics_t m=voice_mic_measure(raw,pcm,640);assert(m.dc>=2047&&m.dc<=2049&&m.raw_rms>=210&&m.raw_rms<=213&&!m.tag_errors);assert(voice_mic_error(pcm,other,640)==10);if(hz==50)assert(m.hum50_permille>970&&m.hum100_permille<3);if(hz==100)assert(m.hum100_permille>970&&m.hum50_permille<3);if(hz==1000)assert(m.hum50_permille<3&&m.hum100_permille<3);}puts("PASS microphone diagnostics: ADC bias/RMS, 50/100Hz discrimination, tag check and codec error");}
