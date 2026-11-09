#include <assert.h>
#include "voice_tx.h"
static bool blocked=true;static unsigned sent[16],count;
static bool send_packet(const uint8_t*p,size_t n){assert(n==1);if(blocked)return false;sent[count++]=*p;return true;}
int main(void){
 voice_tx_t q={0};uint8_t p=1;
 assert(voice_tx_push(&q,&p,1,0));voice_tx_pump(&q,100,send_packet);
 assert(q.count==1&&q.retries==1&&q.dropped==0);
 p=2;voice_tx_push(&q,&p,1,80000);blocked=false;
 voice_tx_pump(&q,160000,send_packet);
 assert(count==2&&sent[0]==1&&sent[1]==2&&q.count==0&&q.sent==2);
 blocked=true;p=3;voice_tx_push(&q,&p,1,200000);
 voice_tx_pump(&q,839999,send_packet);assert(q.count==1);
 voice_tx_pump(&q,840000,send_packet);assert(q.count==0&&q.dropped==1);
 for(p=4;p<4+VOICE_TX_SLOTS+1;p++)voice_tx_push(&q,&p,1,900000);
 assert(q.count==VOICE_TX_SLOTS&&q.dropped==2&&q.items[q.head].data[0]==5);
 // Disconnect, new burst and peer receive all discard pending audio.
 voice_tx_clear(&q);blocked=false;voice_tx_pump(&q,900001,send_packet);
 assert(q.count==0&&q.dropped==2+VOICE_TX_SLOTS&&count==2);
 // A stopped capture can drain its tail without recording new samples.
 p=9;voice_tx_push(&q,&p,1,1000000);voice_tx_pump(&q,1100000,send_packet);
 assert(count==3&&sent[2]==9&&q.count==0);
 assert(!voice_tx_push(&q,&p,VOICE_TX_BYTES+1,1100000));
 return 0;
}
