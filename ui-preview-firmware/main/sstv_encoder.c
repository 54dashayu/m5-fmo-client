#include "sstv_encoder.h"
#include <math.h>
#include <string.h>
void sstv_encoder_init(sstv_encoder_t *e,sstv_row_reader_t read,void *ctx){memset(e,0,sizeof(*e));e->read=read;e->ctx=ctx;e->cached_row=240;}
static float frequency(sstv_encoder_t *e){
 unsigned t=e->sample;
 // Leader, break, leader, VIS start, seven data bits + even parity, stop.
 if(t<SSTV_TX_HEADER_SAMPLES){
  if(t<2400)return 1900;
 if(t<2480)return 1200;
 if(t<4880)return 1900;
  if(t<5120)return 1200;
 if(t<7040)return (0x88u&(1u<<((t-5120)/240)))?1100:1300;
  return 1200;
 }
 t-=SSTV_TX_HEADER_SAMPLES;
 if(t>=240*SSTV_TX_LINE_SAMPLES)return 0;
 unsigned y=t/SSTV_TX_LINE_SAMPLES,p=t%SSTV_TX_LINE_SAMPLES;
 if(p<72)return 1200;
 if(p<96)return 1500;
 if(p>=800&&p<836)return y%2?2300:1500;
 if(p>=836&&p<848)return 1900;
 if(e->cached_row!=y){if(!e->read||!e->read(e->ctx,y,e->row)){e->failed=true;
 return 0;}e->cached_row=y;}
 unsigned x=p<800?(p-96)*320/704:(p-848)*320/352;
 unsigned v=e->row[x*2]|(unsigned)e->row[x*2+1]<<8;
 float r=(v>>11)*255.0f/31,g=((v>>5)&63)*255.0f/63,b=(v&31)*255.0f/31;
 // Studio-range BT.601, matching the existing Robot36 receiver.
 float c=p<800?16+(65.738f*r+129.057f*g+25.064f*b)/256:
  y%2?128+(-37.945f*r-74.494f*g+112.439f*b)/256:128+(112.439f*r-94.154f*g-18.285f*b)/256;
 if(c<0)c=0;
 if(c>255)c=255;
 return 1500+c*800/255;
}
size_t sstv_encoder_read(sstv_encoder_t *e,int16_t *out,size_t n){
 size_t i=0;while(i<n&&e->sample<SSTV_TX_TOTAL_SAMPLES&&!e->failed){float hz=frequency(e);
 if(e->failed)break;
 e->phase+=6.283185307179586f*hz/8000;
 if(e->phase>=6.283185307179586f)e->phase-=6.283185307179586f;
 out[i++]=hz?(int16_t)(10000*sinf(e->phase)):0;e->sample++;}return i;
}
