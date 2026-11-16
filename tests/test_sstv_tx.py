"""Execute the production TX state machine with offline transport and clock."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/voice.c').read_text()
code=s[s.index('static bool sstv_start('):s.index('static void audio_task(')]
pre=r'''
#include "sstv_store.h"
#include "sstv_encoder.h"
#include "voice_wire.h"
#include "voice_tx.h"
#include "voice_sstv_diag.h"
static voice_sstv_diag_t sstv_diag;
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static struct {bool ready,connected,tx;unsigned tx_pending;}state;
static bool requested,test_requested,sstv_cancel,rx_active,missing,blocked,corrupt;
static unsigned sstv_progress,connection_epoch=1,packets,stop_after,disconnect_after;
static uint32_t sstv_job;
static int64_t now,rx_until;
static char last_reason[80];
static int64_t esp_timer_get_time(void){return now;}
static void vTaskDelay(int ms){now+=ms*1000;}
static bool sstv_rx_active(void){return rx_active;}
static void sstv_rx_break(void){}
#ifdef FMO_SSTV_STREAMING
bool sstv_rx_enable(bool on){(void)on;return true;}
#endif
static void audio_stop(void){}
static void notice(const char*s){(void)s;}
static void reason(const char*s){snprintf(last_reason,sizeof(last_reason),"%s",s);}
static bool publish_packet(const uint8_t*p,size_t n){(void)p;(void)n;if(blocked)return false;packets++;if(stop_after&&packets==stop_after)sstv_cancel=true;if(disconnect_after&&packets==disconnect_after){state.connected=false;connection_epoch++;}return true;}
static void tx_status(voice_tx_t*q){state.tx_pending=q->count;}
static void flush(voice_tx_t*q,uint8_t*f,int count,uint32_t base,uint16_t seq){(void)count;(void)base;(void)seq;voice_tx_push(q,f,328,now);voice_tx_pump(q,now,publish_packet);tx_status(q);}
struct sstv_reader {int unused;};static struct sstv_reader reader;
sstv_reader_t*sstv_store_reader_open(uint32_t id){(void)id;return missing?NULL:&reader;}
void sstv_store_reader_close(sstv_reader_t*r){(void)r;}
bool sstv_store_reader_row(void*ctx,unsigned row,uint8_t*p){(void)ctx;if(corrupt&&row==10)return false;memset(p,128,640);return true;}
'''
tail=r'''
static void reset(void){state.ready=state.connected=true;state.tx=false;state.tx_pending=0;requested=test_requested=sstv_cancel=rx_active=missing=blocked=corrupt=false;sstv_job=sstv_progress=packets=stop_after=disconnect_after=0;now=rx_until=0;last_reason[0]=0;}
int main(void){int16_t pcm[640];uint8_t frames[656];voice_tx_t q={0};
 reset();assert(!voice_sstv_start(0));rx_active=true;assert(!voice_sstv_start(1));rx_active=false;requested=true;assert(!voice_sstv_start(1));requested=false;state.connected=false;assert(!voice_sstv_start(1));state.connected=true;
 assert(voice_sstv_start(1));assert(!voice_sstv_start(2));q.dropped=11;q.sent=20;send_sstv(1,connection_epoch,pcm,frames,&q);assert(!strcmp(last_reason,"sstv_complete")&&packets==463&&sstv_progress==100&&!sstv_job&&!state.tx&&q.dropped==11&&q.sent==483);
 reset();q=(voice_tx_t){0};stop_after=5;assert(voice_sstv_start(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(packets==5&&!sstv_job&&!state.tx&&!strcmp(last_reason,"sstv_cancelled"));
 reset();q=(voice_tx_t){0};disconnect_after=5;assert(voice_sstv_start(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(packets==5&&!sstv_job&&!state.tx);
 reset();q=(voice_tx_t){0};blocked=true;assert(voice_sstv_start(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(!packets&&!sstv_job&&q.dropped&&now<2000000&&!q.count);
 reset();missing=true;assert(voice_sstv_start(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(!packets&&!sstv_job&&!state.tx);
 reset();corrupt=true;assert(voice_sstv_start(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(packets>0&&packets<463&&!sstv_job&&!state.tx);
 reset();assert(voice_sstv_start(1));voice_sstv_stop();send_sstv(1,connection_epoch,pcm,frames,&q);assert(!packets&&!sstv_job);
 reset();q=(voice_tx_t){0};assert(voice_sstv_probe(1));send_sstv(1,connection_epoch,pcm,frames,&q);assert(!packets&&sstv_diag.generated==463&&sstv_diag.submitted==0&&!state.tx&&!strcmp(last_reason,"sstv_probe_complete"));
 puts("PASS actual SSTV TX: 463 timed packets, counter continuity, busy rejection, cancel, disconnect, congestion, missing/corrupt source");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'tx.c';p.write_text(pre+code+tail)
 for flags in ([],['-DCONFIG_IDF_TARGET_ESP32'],['-DCONFIG_FMO_BOARD_XINGZHI']):
  subprocess.run(['cc',*flags,'-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(p),str(root/'ui-preview-firmware/main/sstv_encoder.c'),str(root/'ui-preview-firmware/main/voice_wire.c'),'-lm','-o',td+'/test'],check=True)
  subprocess.run([td+'/test'],check=True)
