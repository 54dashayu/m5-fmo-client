"""Run M5's actual refill branch against jitter, bounded wait and pressure."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/voice.c').read_text()
a=s.index('  if(underrun&&pending_audio)');branch=s[a:s.index('\n#endif',a)]
pre=r'''
#include "voice_rx.h"
#include <assert.h>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define xSemaphoreTake(x,y) ((void)0)
#define xSemaphoreGive(x) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static voice_rx_t rx_buffer;
static struct{unsigned rx_rebuffers;}state;
static int64_t now;
static int64_t esp_timer_get_time(void){return now;}
static void play(void*p,int n){(void)p;(void)n;}
static void vTaskDelay(int n){(void)n;}
static bool step(bool underrun,unsigned buffer_ms){
 bool pending_audio=rx_buffer.count>0;int mode=0;
 for(int once=0;once<1;once++){
'''
post=r'''
 return true;
 }return false;
}
int main(void){
 uint8_t data[2048]={0};
 assert(step(true,240));assert(state.rx_rebuffers==0);
 voice_rx_push(&rx_buffer,data,100,320,1000000);
 now=1100000;assert(!step(true,240)); // refill after starvation
 now=1240000;assert(step(true,240)); // bounded even if no more data arrives
 assert(state.rx_rebuffers==1);
 now=1100000;assert(step(false,240)); // uninterrupted playback has no extra wait
 for(int i=0;i<5;i++)voice_rx_push(&rx_buffer,data,100,320,1000000);
 assert(step(true,240)); // 240ms accumulated: resume immediately
 assert(!step(true,480)); // SSTV keeps a larger jitter cushion
 voice_rx_clear(&rx_buffer);voice_rx_push(&rx_buffer,data,530,4480,1000000);
 now=1100000;assert(!step(true,480)); // one 560ms batch must not bypass jitter buffering
 now=1480000;assert(step(true,480)); // single packet tail remains bounded
 now=1100000;voice_rx_push(&rx_buffer,data,100,320,1000000);assert(step(true,480));
 voice_rx_clear(&rx_buffer);voice_rx_push(&rx_buffer,data,2048,320,1000000);
 voice_rx_push(&rx_buffer,data,2048,320,1000000);
 assert(step(true,480)); // avoid overflow under pressure
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+branch+post)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(r/'ui-preview-firmware/main'),str(p),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
print('PASS production M5 refill: jitter recovery, bounded wait, continuous audio, SSTV and memory pressure')
