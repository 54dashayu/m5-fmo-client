#include "sstv_decoder.h"
#include <math.h>
#include <string.h>
static int clamp(int x){return x<0?0:x>255?255:x;}
static int level(float f){return clamp((int)((f-1500)*255/800+0.5f));}
static uint16_t rgb(int y,int cb,int cr){
    int r=clamp((298*(y-16)+409*(cr-128)+128)>>8);
    int g=clamp((298*(y-16)-100*(cb-128)-208*(cr-128)+128)>>8);
    int b=clamp((298*(y-16)+516*(cb-128)+128)>>8);
#ifdef FMO_FULL_SSTV
    return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
#else
    return (r&224)|((g>>3)&28)|(b>>6);
#endif
}
void sstv_decoder_init(sstv_decoder_t*d){
    memset(d,0,sizeof(*d));
    for(int k=0;k<31;k++){int n=k-15;if(n&&n%2)d->taps[k]=2.0f/(3.14159265358979323846f*n)*(0.54f-0.46f*cosf(2*3.14159265358979323846f*k/30));}
}
void sstv_decoder_break(sstv_decoder_t*d){
    if(d->status==SSTV_RECEIVING)d->status=SSTV_INTERRUPTED;
    memset(d->fir,0,sizeof(d->fir));d->prev_i=d->prev_q=d->smooth=0;
    d->leader=d->leader_gap=d->sync_run=d->header_deadline=0;d->armed=d->vis=d->scanning=d->pair=false;
}
static void finish_line(sstv_decoder_t*d){
    float sep=0;for(int i=740;i<756;i++)sep+=d->line[i];sep/=16;
    bool even=(d->lines%2)==0;
    /* Reject a broken separator instead of silently swapping chroma. */
    if(fabsf(sep-(even?1500:2300))>180){sstv_decoder_break(d);return;}
    for(int x=0;x<SSTV_WIDTH;x++){
        int yi=24+(x*704+352)/SSTV_WIDTH;
        int ci=776+(x*352+176)/SSTV_WIDTH;
        int y=level((d->line[yi]+d->line[yi+1])*0.5f);
        int c=level((d->line[ci]+d->line[ci+1])*0.5f);
        if(even){d->even_y[x]=y;d->even_cr[x]=c;}
        else if(d->pair){
#ifdef FMO_FULL_SSTV
            unsigned at=
#ifdef FMO_SSTV_STREAMING
            x*2;
#else
            ((d->lines-1)*SSTV_WIDTH+x)*2;
#endif
            uint16_t a=rgb(d->even_y[x],c,d->even_cr[x]),b=rgb(y,c,d->even_cr[x]);
            d->image[at]=a;d->image[at+1]=a>>8;at+=SSTV_WIDTH*2;d->image[at]=b;d->image[at+1]=b>>8;
#else
            d->image[(d->lines/2)*SSTV_WIDTH+x]=rgb((y+d->even_y[x])/2,c,d->even_cr[x]);
#endif
        }
    }
#ifdef FMO_SSTV_STREAMING
    if(!even&&d->pair&&d->rows&&!d->rows(d->sink,d->lines-1,d->image)){sstv_decoder_break(d);return;}
#endif
    d->pair=even;d->lines++;d->generation++;
    if(d->lines==240){d->status=SSTV_COMPLETE;d->scanning=false;}
}
static void sample(sstv_decoder_t*d,float f){
    d->clock++;
    d->smooth+=0.25f*(f-d->smooth);
    float t=d->smooth;
    if(d->status==SSTV_RECEIVING){
        if(d->scanning){unsigned p=d->clock-d->line_start;if(p<1140)d->line[p]=t;
            if(p==1130){finish_line(d);d->scanning=false;}}
        if(d->clock-d->line_start>2000)sstv_decoder_break(d);
        if(t>1060&&t<1340)d->sync_run++;
        else if(d->sync_run){unsigned n=d->sync_run;d->sync_run=0;
            if(n>=40&&n<=108){
                if(d->scanning){sstv_decoder_break(d);return;}
                if(d->lines&& (d->clock-d->line_start<1160||d->clock-d->line_start>1240)){sstv_decoder_break(d);return;}
                /* Use the same filtered frequency for sync and pixels. */
                d->line_start=d->clock;d->scanning=true;memset(d->line,0,sizeof(d->line));
            }}
        return;
    }
    if(d->vis){
        unsigned elapsed=d->clock-d->vis_start,bit=elapsed/240,phase=elapsed%240;
        if(bit>8){d->vis=false;if(d->vis_bit){
#ifdef FMO_SSTV_STREAMING
            if(d->begin&&!d->begin(d->sink)){sstv_decoder_break(d);d->status=SSTV_INTERRUPTED;return;}
#endif
            memset(d->image,0,sizeof(d->image));d->lines=0;d->pair=false;d->scanning=false;
            d->status=SSTV_RECEIVING;d->generation++;d->line_start=d->clock;d->sync_run=0;
        }return;}
        if(phase>=72&&phase<168){d->vis_sum+=t;d->vis_count++;}
        if(phase==168){float avg=d->vis_count?d->vis_sum/d->vis_count:0;d->vis_sum=0;d->vis_count=0;
            if(bit<8){int value=fabsf(avg-1100)<60?1:fabsf(avg-1300)<60?0:-1;
                if(value<0){d->vis=false;return;}d->vis_code|=(unsigned)value<<bit;
            }else{
                /* Seven data bits + even parity; Robot36 is 0x08 => 0x88 on wire. */
                if(d->vis_code==0x88&&fabsf(avg-1200)<60){
                    d->vis_bit=1;
                }
            }
        }
        return;
    }
    if(t>1830&&t<1970){d->leader_gap=0;if(++d->leader>=2000){d->armed=true;d->header_deadline=d->clock+6400;}}
#ifdef FMO_FULL_SSTV
    /* RADPCM can move individual estimates outside the leader band. Require
     * 250 ms of in-band samples, tolerating only isolated <=1 ms glitches. */
    else if(++d->leader_gap>8)d->leader=0;
#else
    else d->leader=0;
#endif
    if(d->leader>d->leader_max)d->leader_max=d->leader;
    if(d->armed&&(int32_t)(d->clock-d->header_deadline)>0)d->armed=false;
    if(t>1130&&t<1270)d->sync_run++;
    else if(d->sync_run){unsigned n=d->sync_run;d->sync_run=0;
        if(d->armed&&n>=190&&n<=290){d->vis_starts++;d->vis=true;d->armed=false;d->vis_start=d->clock;d->vis_code=0;d->vis_bit=0;d->vis_sum=0;d->vis_count=0;}}
}
void sstv_decoder_feed(sstv_decoder_t*d,const int16_t*pcm,size_t count){
    for(size_t j=0;j<count;j++){
        d->fir[d->pos]=pcm[j];float q=0;
        for(unsigned k=0;k<31;k++)q+=d->taps[k]*d->fir[(d->pos+31-k)%31];
        float i=d->fir[(d->pos+31-15)%31];d->pos=(d->pos+1)%31;
        float cross=d->prev_i*q-d->prev_q*i,dot=d->prev_i*i+d->prev_q*q;
        float f=(i*i+q*q>10000&&d->prev_i*d->prev_i+d->prev_q*d->prev_q>10000)?atan2f(cross,dot)*1273.239544735f:0;
        d->prev_i=i;d->prev_q=q;sample(d,f);
    }
}
