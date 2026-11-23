#include "sstv_encoder.h"
#include "sstv_decoder.h"
#include "voice_wire.h"
#include <assert.h>
#include <stdio.h>
static sstv_decoder_t decoder;
static bool row(void *ctx,unsigned y,uint8_t *out){(void)ctx;assert(y<240);const unsigned colors[]={0xffff,0,0xf800,0x07e0,0x001f,0x8410};for(unsigned x=0;x<320;x++){unsigned v=colors[x*6/320];out[x*2]=v;out[x*2+1]=v>>8;}return true;}
static bool bad_row(void *ctx,unsigned y,uint8_t *out){(void)ctx;(void)y;(void)out;return false;}
int main(void){
 for(int codec=0;codec<2;codec++){
  sstv_encoder_t e;sstv_encoder_init(&e,row,NULL);sstv_decoder_init(&decoder);int predictor=0,index=0,frame=0;unsigned total=0;int peak=0;int16_t pcm[640];
  while(e.sample<SSTV_TX_TOTAL_SAMPLES){size_t n=sstv_encoder_read(&e,pcm,640);assert(n&&!e.failed);total+=n;for(size_t k=0;k<n;k++){int a=pcm[k]<0?-pcm[k]:pcm[k];assert(a<=5000);if(a>peak)peak=a;}for(size_t k=n;k<640;k++)pcm[k]=0;
   if(codec){uint8_t wire[328];radpcm_encode(pcm,wire,frame++,&predictor,&index);assert(radpcm_decode(wire,pcm));}
   sstv_decoder_feed(&decoder,pcm,640);
  }
  assert(peak>=4990&&peak<=5000);assert(total==SSTV_TX_TOTAL_SAMPLES);assert(!sstv_encoder_read(&e,pcm,640));
  printf("codec %d: %u samples, %u lines, state %d\n",codec,total,decoder.lines,decoder.status);
  assert(decoder.status==SSTV_COMPLETE&&decoder.lines==240);
  for(unsigned y=10;y<110;y++){unsigned ri=(y*2*320+132)*2,gi=(y*2*320+186)*2,bi=(y*2*320+240)*2;unsigned rv=decoder.image[ri]|decoder.image[ri+1]<<8,gv=decoder.image[gi]|decoder.image[gi+1]<<8,bv=decoder.image[bi]|decoder.image[bi+1]<<8;unsigned r=((rv>>8)&224)|((rv>>6)&28)|((rv>>3)&3),g=((gv>>8)&224)|((gv>>6)&28)|((gv>>3)&3),b=((bv>>8)&224)|((bv>>6)&28)|((bv>>3)&3);assert((r&224)>=192&&(r&28)<=8);assert((g&28)>=24&&(g&224)<=64);assert((b&3)>=2&&(b&224)<=64);}
 }
 sstv_encoder_t e;sstv_encoder_init(&e,bad_row,NULL);int16_t pcm[640];while(sstv_encoder_read(&e,pcm,640)){}assert(e.failed);puts("Robot36 TX: PCM/RADPCM colors, VIS, length, EOF and reader failure passed");
}
