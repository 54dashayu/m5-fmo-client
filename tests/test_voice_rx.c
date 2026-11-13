#include "voice_rx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct{uint8_t bytes[VOICE_RX_PACKET_MAX];unsigned n,samples;int64_t arrival;} model_t;
int main(void){
 assert(sizeof(voice_rx_t)<=4*1416);
 voice_rx_t q={0};uint8_t in[VOICE_RX_PACKET_MAX],out[VOICE_RX_PACKET_MAX];uint16_t n;int64_t arrival;
 // Typical short 3x40ms Opus packets: more than four fit with no loss.
 for(int i=0;i<12;i++){memset(in,i,240);assert(voice_rx_push(&q,in,240,960,i)==0);}
 assert(q.count==12);assert(!voice_rx_prebuffer(&q,10000));
 for(int i=0;i<12;i++){assert(voice_rx_pop(&q,out,&n,&arrival));assert(n==240&&arrival==i);for(int j=0;j<n;j++)assert(out[j]==i);}
 assert(!voice_rx_pop(&q,out,&n,&arrival));
 voice_rx_clear(&q);assert(voice_rx_push(&q,in,240,320,0)==0);assert(voice_rx_prebuffer(&q,100000));assert(!voice_rx_prebuffer(&q,240000));
 voice_rx_clear(&q);for(int i=0;i<3;i++)voice_rx_push(&q,in,1200,320,i);assert(!voice_rx_prebuffer(&q,100000));voice_rx_push(&q,in,1200,320,4);assert(!voice_rx_prebuffer(&q,100000));
 // SSTV reserves 480ms within the same fixed buffer; ordinary voice remains 240ms.
 voice_rx_clear(&q);for(int i=0;i<3;i++)voice_rx_push(&q,in,408,640,i);
 assert(!voice_rx_prebuffer(&q,200000));assert(voice_rx_prebuffer_for(&q,200000,480));
 for(int i=0;i<3;i++)voice_rx_push(&q,in,408,640,i);
 assert(!voice_rx_prebuffer_for(&q,200000,480));assert(!voice_rx_prebuffer_for(&q,480000,480));
 // Random byte/slot pressure and wrapped reads compared with a FIFO model.
 voice_rx_clear(&q);model_t model[24];unsigned count=0,bytes=0,samples=0;srand(719);
 for(int t=0;t<20000;t++){
  if(rand()%3!=0){unsigned len=80+rand()%(VOICE_RX_PACKET_MAX-79),ns=160*(1+rand()%24);memset(in,t%251,len);unsigned drops=0;
   while(count==24||bytes+len>5120){bytes-=model[0].n;samples-=model[0].samples;memmove(model,model+1,(--count)*sizeof(*model));drops++;}
   assert(voice_rx_push(&q,in,len,ns,t)==(int)drops);model[count]=(model_t){.n=len,.samples=ns,.arrival=t};memcpy(model[count++].bytes,in,len);bytes+=len;samples+=ns;
  }else{bool ok=voice_rx_pop(&q,out,&n,&arrival);assert(ok==(count>0));if(ok){assert(n==model[0].n&&arrival==model[0].arrival&&!memcmp(out,model[0].bytes,n));bytes-=n;samples-=model[0].samples;memmove(model,model+1,(--count)*sizeof(*model));}}
  assert(q.count==count&&q.used==bytes&&q.samples==samples);
 }
 voice_rx_clear(&q);assert(q.count==0&&q.used==0&&q.samples==0);assert(voice_rx_push(&q,in,VOICE_RX_PACKET_MAX+1,640,0)==-1);assert(q.count==0);
 puts("PASS compact RX buffer: short Opus burst, prebuffer pressure, wrap, eviction, reset, fixed memory budget");
}
