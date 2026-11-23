from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/boards/cyd28_plus/tone_probe.inc').read_text();s=s[s.index('static esp_err_t cyd_pcm_tone'):]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef struct {int volume;bool muted;uint32_t output_errors;}voice_view_t;
static voice_view_t v={100,false,0};static int samples,silence,blocks,fail_mode,fail_write,mute_at;
static void voice_view(voice_view_t*out){*out=v;if(mute_at&&blocks>=mute_at)out->muted=true;}
static int audio_mode(bool mic){assert(!mic);return fail_mode?-2:0;}
static void notice(const char*s){(void)s;}
static void play(const int16_t*p,int n){assert(n>0);if(!p){silence+=n;return;}assert(n==160);
 for(int i=0;i<n;i++){assert(p[i]>=-1800&&p[i]<=1800);if(samples==0&&i==0)assert(p[i]==0);if(samples==39840&&i==159)assert(p[i]==0);if(samples>=160&&samples<39840){assert(p[0]==0&&p[4]==1800&&p[12]==-1800);}}
 samples+=n;blocks++;if(fail_write)v.output_errors++;
}
'''
tail=r'''
int main(void){assert(cyd_pcm_tone()==0);assert(samples==40000&&blocks==250&&silence==640);
 samples=blocks=silence=0;mute_at=3;assert(cyd_pcm_tone()==0);assert(samples==480&&silence==640);
 mute_at=0;fail_mode=1;samples=0;assert(cyd_pcm_tone()==-2&&samples==0);
 fail_mode=0;fail_write=1;assert(cyd_pcm_tone()==-1);
 v.muted=true;samples=0;assert(cyd_pcm_tone()==0&&samples==0);
 v.muted=false;v.volume=0;assert(cyd_pcm_tone()==0&&samples==0);
 puts("PASS local PCM through play: 40000 samples / 5s, 500Hz, bounded amplitude, onset/offset ramps, mute and failure reporting");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'pcm.c';p.write_text(pre+s+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/pcm'],check=True);subprocess.run([td+'/pcm'],check=True)
