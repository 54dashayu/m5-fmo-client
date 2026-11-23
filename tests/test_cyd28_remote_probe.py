"""Production USB-queued UI probe and backlight restoration state-machine tests."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
display=(r/'ui-preview-firmware/components/lv_porting/cyd28_plus_display.c').read_text();display=display[display.index('static int requested_brightness'):display.index('void lv_port_disp_init')]
probe=(r/'ui-preview-firmware/main/boards/cyd28_plus/remote_probe.inc').read_text().replace('#include "boards/cyd28_plus/remote_probe.h"','')
pre=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
static int forced=-1,tones,busy,active;
#define ESP_ERROR_CHECK(x) assert((x)==0)
static int ledc_stop(int m,int c,int idle){(void)m;(void)c;assert(idle==0||idle==1);forced=idle;return 0;}
typedef struct {bool ready,muted;int volume;}voice_view_t;
static voice_view_t view={true,false,25};
static void voice_view(voice_view_t*v){*v=view;}
static bool voice_busy(void){return busy;}
static void voice_tone(void){tones++;}
static void cyd_voice_pcm_tone(void){tones++;}
static bool cyd_voice_tone_active(void){return active;}
'''
tail=r'''
int main(void){
 disp_driver_set_brightness(100);assert(forced==1);
 for(int p=-10;p<=110;p++){disp_driver_set_brightness(p);assert(forced==(p>0));}
 disp_driver_set_brightness(100);
 assert(!cyd_remote_probe_request(-1)&&!cyd_remote_probe_request(3));
 view.muted=true;assert(!cyd_remote_probe_request(0));view.muted=false;
 assert(cyd_remote_probe_request(0));assert(!cyd_remote_probe_request(1));
 cyd_remote_probe_tick(0);assert(forced==0&&tones==1&&cyd_remote_probe_status()==2);
 disp_driver_set_brightness(30);assert(forced==0); // normal idle policy cannot override probe
 cyd_remote_probe_tick(100);assert(cyd_remote_probe_status()==2);
 active=true;cyd_remote_probe_tick(500000);active=false;cyd_remote_probe_tick(5500000);
 assert(cyd_remote_probe_status()==3&&forced==1);
 assert(cyd_remote_probe_request(1));cyd_remote_probe_tick(6000000);assert(forced==1);
 cyd_remote_probe_tick(16000000);assert(cyd_remote_probe_status()==5&&forced==1); // timeout restores
 assert(cyd_remote_probe_request(0));busy=true;cyd_remote_probe_tick(17000000);
 assert(cyd_remote_probe_status()==4&&tones==2&&forced==1);busy=false;
 assert(cyd_remote_probe_request(2));cyd_remote_probe_tick(18000000);assert(forced==1);
 disp_driver_set_brightness(0);active=true;cyd_remote_probe_tick(19000000);active=false;cyd_remote_probe_tick(24000000);assert(forced==0);
 disp_driver_set_brightness(100);assert(forced==1);
 puts("PASS steady backlight all levels, sleep/wake and USB probe: validation, duplicate/busy rejection, UI-only light override, actual-tone completion, timeout and brightness restore");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'probe.c';p.write_text(pre+display+probe+tail)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',td+'/probe'],check=True)
 subprocess.run([td+'/probe'],check=True)
web=(r/'ui-preview-firmware/main/device_web.c').read_text()
assert 'if(r->usb&&(!strcmp(op,"audio_probe")||!strcmp(op,"audio_probe_pcm"))){if(!auth(r))' in web
assert 'if(r->usb&&!strcmp(op,"audio_probe_status")){if(!auth(r))' in web
assert '"/api/audio/probe"' not in web # no LAN HTTP route
print('PASS probe commands confined to authenticated USB dispatch')
