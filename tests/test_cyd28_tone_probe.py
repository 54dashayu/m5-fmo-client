"""Execute production hardware-tone sequencing and failure cleanup without hardware."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/boards/cyd28_plus/tone_probe.inc').read_text()
s=s[:s.index('// Local PCM')]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define DAC_CHAN_1 1
#define DAC_CW_SCALE_8 3
#define DAC_CW_PHASE_0 2
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGI(...) ((void)0)
typedef struct {int volume;bool muted;} voice_view_t;
typedef struct {int en_ch,scale,phase,freq,offset;} dac_cw_config_t;
static int amp,phase,sounding,elapsed,stop_count,fail,muted,volume=25,mute_after;
static void voice_view(voice_view_t *v){v->volume=volume;v->muted=muted||(mute_after&&elapsed>=mute_after);}
static void gpio_set_level(int pin,int level){assert(pin==4);amp=level;}
static void audio_stop(void){assert(amp==1);phase=1;stop_count++;}
static int dac_output_enable(int ch){assert(ch==1&&phase==1);phase=2;return fail==1?-1:0;}
static int dac_output_voltage(int ch,int val){assert(ch==1&&val==128&&amp==1);return 0;}
static int dac_cw_generator_config(const dac_cw_config_t*c){assert(phase==2&&amp==1);assert(c->en_ch==1&&c->scale==3&&c->phase==2&&c->freq==800&&c->offset==0);phase=3;return fail==2?-1:0;}
static int dac_cw_generator_enable(void){assert(phase==3&&amp==1);if(fail==3)return -1;sounding=1;return 0;}
static int dac_cw_generator_disable(void){assert(sounding&&amp==1);sounding=0;return 0;}
static void vTaskDelay(int ms){if(sounding){assert(amp==0&&ms==50);elapsed+=ms;}else assert(amp==1&&ms==150);}
static void notice(const char*x){(void)x;}
'''
tail=r'''
int main(void){
 for(fail=0;fail<=3;fail++){amp=0;phase=0;elapsed=0;stop_count=0;sounding=0;assert((cyd_hardware_tone()==0)==(fail==0));assert(!sounding&&amp==0&&stop_count==1);assert(elapsed==(fail?0:5000));}
 fail=0;elapsed=0;mute_after=150;assert(cyd_hardware_tone()==0);assert(elapsed==150&&!sounding&&amp==0);
 mute_after=0;muted=1;stop_count=0;assert(cyd_hardware_tone()==0&&stop_count==0);
 muted=0;volume=0;assert(cyd_hardware_tone()==0&&stop_count==0);
 puts("PASS hardware DAC2 tone: 5 seconds, min amplitude, no I2S writes, mute/zero, startup failures and cleanup");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'probe.c';p.write_text(pre+s+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/probe'],check=True)
 subprocess.run([td+'/probe'],check=True)
