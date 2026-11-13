"""Execute the production playback-starvation branch, including late rejection."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/voice.c').read_text()
branch=s[s.index('  bool available=receive_packet('):s.index('  underrun=false;voice_packet_t')]
pre=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static struct{int64_t arrival;}packet;
static struct{unsigned rx_underflows,dropped,rx_late;}state;
static unsigned breaks,reads,silence;static bool stale;
static bool receive_packet(void*p,unsigned wait){(void)p;(void)wait;return reads++!=0;}
static int64_t esp_timer_get_time(void){return stale?1000001:260000;}
static void sstv_rx_break(void){breaks++;}
static void play(void*p,int n){(void)p;(void)n;silence++;}
static void audio_stop(void){}static void vTaskDelay(int n){(void)n;}
static void run(void){bool receiving=true,seen=true,underrun=false;int mode=0;int64_t silence_until=2000000;
 for(int i=0;i<2;i++){
'''
post=r'''
 }}
int main(void){run();assert(state.rx_underflows==1&&silence==1&&breaks==0);reads=0;stale=true;run();assert(breaks==1&&state.rx_late==1);puts("PASS production playback branch: 260ms jitter preserves SSTV; stale packet invalidates");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+branch+post)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
