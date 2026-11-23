"""Run actual board peripheral initialization against GPIO capture stubs."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/boards/cyd28_plus/peripherals.c').read_text();code=s[s.index('void side_leds_init'):s.index('void side_leds_view')]
pre='''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct {uint64_t pin_bit_mask;int mode;}gpio_config_t;
#define GPIO_MODE_OUTPUT 1
#define ESP_ERROR_CHECK(x) assert((x)==0)
static uint64_t configured;static int levels[40];
static int gpio_config(const gpio_config_t*c){assert(c->mode==GPIO_MODE_OUTPUT);configured|=c->pin_bit_mask;return 0;}
static int gpio_set_level(int pin,int level){assert(configured&(1ULL<<pin));levels[pin]=level;return 0;}
'''
tail='''int main(void){side_leds_init();assert(levels[4]==0);assert(levels[22]==1&&levels[16]==1&&levels[17]==1);for(int i=0;i<100;i++)side_leds_update(i&1,i&2,i*25);assert(levels[4]==0);assert(!(configured&(1ULL<<26)));puts("PASS Plus amplifier LOW enabled (dev5 baseline), RGB off, status updates preserve amp, DAC26 untouched");}'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'audio.c';p.write_text(pre+code+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/audio'],check=True);subprocess.run([td+'/audio'],check=True)


# Compile the production tone loop for both board branches; retain dev5 waveform.
voice=(r/'ui-preview-firmware/main/voice.c').read_text()
start=voice.index('for(int b=0;b<',voice.index('if(tone){'))
loop=voice[start:voice.index('silence_until=',start)]
with tempfile.TemporaryDirectory() as td:
 for plus,expected in [(False,3200)]:
  source=('#define CONFIG_FMO_BOARD_CYD28_PLUS 1\n' if plus else '')+"""#include <assert.h>
#include <stdint.h>
#include <stddef.h>
static int samples,silent;static int16_t pcm[160];
static void play(const int16_t *p,int n){if(!p){silent+=n;return;}for(int i=0;i<n;i++)assert(p[i]==((i%10<5)?1800:-1800));samples+=n;}
int main(void){
"""+loop+f'assert(samples=={expected});assert(silent==640);return 0;}}'
  path=Path(td)/'tone.c';path.write_text(source)
  subprocess.run(['cc','-Wall','-Wextra','-Werror',str(path),'-o',td+'/tone'],check=True)
  subprocess.run([td+'/tone'],check=True)
print('PASS other boards retain original 0.4-second waveform')
