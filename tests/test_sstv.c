#include "sstv_decoder.h"
#include "voice_wire.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef SSTV_TEST_OPUS
#include <opus.h>
static OpusEncoder *encoder;static OpusDecoder *opus;
#endif
static sstv_decoder_t d;
static unsigned pixel(int x,int y){
#ifdef FMO_FULL_SSTV
 unsigned at=((y*2)*320+x*2)*2,v=d.image[at]|(d.image[at+1]<<8);return ((v>>8)&224)|((v>>6)&28)|((v>>3)&3);
#else
 return d.image[y*160+x];
#endif
}
static double phase;
static int16_t block[640];static int used,compressed,predictor,step,frame;
static void flush(void){
    if(!used)return;
    if(compressed==1){assert(used==640);uint8_t raw[328];radpcm_encode(block,raw,frame++,&predictor,&step);assert(radpcm_decode(raw,block));}
#ifdef SSTV_TEST_OPUS
    if(compressed==2){assert(used==640);for(int k=0;k<640;k+=160){unsigned char packet[512];int n=opus_encode(encoder,block+k,160,packet,sizeof(packet));assert(n>0);assert(opus_decode(opus,packet,n,block+k,160,0)==160);}}
#endif
    sstv_decoder_feed(&d,block,used);used=0;
}
static void tone(float hz,int n){for(int i=0;i<n;i++){phase+=6.283185307179586*hz/8000;block[used++]=hz?(int16_t)(10000*sin(phase)):0;if(used==640)flush();}}
static void header(unsigned code){tone(1900,2400);tone(1200,80);tone(1900,2400);tone(1200,240);for(unsigned b=0;b<8;b++)tone(code&(1u<<b)?1100:1300,240);tone(1200,240);}
static void line(int row){tone(1200,72);tone(1500,24);tone(1500+126*800.0f/255,704);tone(row%2?2300:1500,36);tone(1900,12);tone(1500+128*800.0f/255,352);}
static const int patches[6][3]={{235,128,128},{16,128,128},{81,90,240},{145,54,34},{41,240,110},{126,128,128}};
static void color_line(int row){
    tone(1200,72);tone(1500,24);
    for(int i=0;i<704;i++)tone(1500+patches[i*6/704][0]*800.0f/255,1);
    tone(row%2?2300:1500,36);tone(1900,12);
    for(int i=0;i<352;i++)tone(1500+patches[i*6/352][row%2?1:2]*800.0f/255,1);
}
static void end(void){tone(0,640-used);flush();}
static void picture(void){header(0x88);for(int r=0;r<240;r++)line(r);end();}
int main(void){
    int codecs=2;
#ifdef FMO_FULL_SSTV
    compressed=1;sstv_decoder_init(&d);picture();assert(d.status==SSTV_COMPLETE&&d.lines==240);
    phase=0;predictor=step=frame=used=0;
#endif
#ifdef SSTV_TEST_OPUS
    int err;encoder=opus_encoder_create(8000,1,OPUS_APPLICATION_AUDIO,&err);assert(encoder&&err==OPUS_OK);
    opus=opus_decoder_create(8000,1,&err);assert(opus&&err==OPUS_OK);opus_encoder_ctl(encoder,OPUS_SET_BITRATE(32000));codecs=3;
#endif
    for(compressed=0;compressed<codecs;compressed++){
        sstv_decoder_init(&d);picture();
        printf("codec=%d status=%d lines=%u rgb=%02x\n",compressed,d.status,d.lines,d.image[100]);
        assert(d.status==SSTV_COMPLETE&&d.lines==240);
        long error=0;int max_error=0;
        for(int y=0;y<120;y++)for(int x=8;x<152;x++){int p=pixel(x,y);int e=abs((p&224)+16-128)+abs(((p&28)<<3)+16-128)+abs(((p&3)<<6)+32-128);error+=e;if(e>max_error)max_error=e;}
        printf("mean channel error=%.2f max sum=%d\n",error/(120.0*144*3),max_error);
        assert(error/(120.0*144*3)<30);
        sstv_decoder_break(&d);assert(d.status==SSTV_COMPLETE);assert(d.image[100]);
    }
    for(compressed=0;compressed<codecs;compressed++){
        sstv_decoder_init(&d);header(0x88);for(int r=0;r<240;r++)color_line(r);end();
        assert(d.status==SSTV_COMPLETE&&d.lines==240);
        for(int y=1;y<120;y++){
            unsigned white=pixel(13,y),black=pixel(40,y),red=pixel(66,y),green=pixel(93,y),blue=pixel(120,y);
            assert((white&224)>=192&&(white&28)>=24&&(white&3)>=2);
            assert((black&224)<=32&&(black&28)<=4&&(black&3)<=1);
            assert((red&224)>=192&&(red&28)<=8&&(red&3)<=1);
            assert((green&224)<=64&&(green&28)>=24&&(green&3)<=1);
            assert((blue&224)<=64&&(blue&28)<=8&&(blue&3)>=2);
        }
        printf("codec=%d color bars passed\n",compressed);
    }
    compressed=0;sstv_decoder_init(&d);header(0x08);for(int r=0;r<4;r++)line(r);end();assert(d.status==SSTV_WAITING);
    sstv_decoder_init(&d);header(0xac);for(int r=0;r<4;r++)line(r);end();assert(d.status==SSTV_WAITING);
    sstv_decoder_init(&d);header(0x88);for(int r=0;r<4;r++)line(r);end();assert(d.lines==4);sstv_decoder_break(&d);assert(d.status==SSTV_INTERRUPTED);for(int r=4;r<10;r++)line(r);end();assert(d.lines==4);
    sstv_decoder_init(&d);header(0x88);line(0);tone(0,4000);end();assert(d.status==SSTV_INTERRUPTED);
    sstv_decoder_init(&d);for(int k=0;k<40;k++){tone(1900,800);tone(1200,80);tone(1300,800);}end();assert(d.status==SSTV_WAITING);
#ifdef FMO_FULL_SSTV
    sstv_decoder_init(&d);header(0x88);for(int row=0;row<240;row++){
      tone(1200,72);tone(1500,24);tone(1500+(row%2?16:235)*800.0f/255,704);tone(row%2?2300:1500,36);tone(1900,12);tone(1500+128*800.0f/255,352);
    }end();assert(d.status==SSTV_COMPLETE);
    for(int row=0;row<240;row++){unsigned at=(row*320+100)*2,v=d.image[at]|(d.image[at+1]<<8);if(row%2)assert((v>>11)<4);else assert((v>>11)>27);}
    puts("PASS native 320x240: separate even/odd luminance rows preserved");
#endif
    puts("SSTV: PCM/RADPCM full frames, parity, unsupported VIS, interruption and false-trigger checks passed");
    return 0;
}
