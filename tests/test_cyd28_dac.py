"""Run the production dedicated DAC transport with driver stubs and byte capture."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];code=(r/'ui-preview-firmware/main/boards/cyd28_plus/audio.inc').read_text()
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "voice_dsp.h"
#include "boards/cyd28_plus/audio_output.h"
#define CONFIG_DAC_DMA_AUTO_16BIT_ALIGN 1
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define portENTER_CRITICAL_ISR(x) ((void)0)
#define portEXIT_CRITICAL_ISR(x) ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define ESP_OK 0
#define ESP_ERR_NOT_SUPPORTED 1
#define ESP_ERR_TIMEOUT 2
#define DAC_CHANNEL_MASK_CH1 2
#define DAC_DIGI_CLK_SRC_APLL 1
#define DAC_CHANNEL_MODE_SIMUL 0
#define MALLOC_CAP_DMA 1
typedef int esp_err_t;typedef void* dac_continuous_handle_t;
typedef struct {int chan_mask,desc_num,buf_size,freq_hz,offset,clk_src,chan_mode;}dac_continuous_config_t;
typedef struct {size_t write_bytes;}dac_event_data_t;
typedef struct {bool(*on_convert_done)(dac_continuous_handle_t,const dac_event_data_t*,void*);bool(*on_stop)(dac_continuous_handle_t,const dac_event_data_t*,void*);}dac_event_callbacks_t;
typedef struct {bool muted;int volume;unsigned dac_dma_stops,play_gap_us_max,play_late_feeds,pcm_samples_written,output_errors;}voice_view_t;
static voice_view_t state={.volume=45};static int mode=-1;static int64_t last_pcm_feed,clock_us;
static int amp=1,fail,allocated,enabled,creates,writes;static bool short_write;static uint8_t captured[40000];static size_t captured_n;static dac_event_callbacks_t callbacks;
static void voice_view(voice_view_t*v){*v=state;}
static int64_t esp_timer_get_time(void){return clock_us;}
static void gpio_set_level(int pin,int v){assert(pin==4);amp=v;}
static void vTaskDelay(int ms){assert(amp==1);clock_us+=ms*1000;}
static int dac_continuous_new_channels(const dac_continuous_config_t*c,dac_continuous_handle_t*h){
 assert(amp==1&&!allocated&&c->chan_mask==2&&c->desc_num==6&&c->buf_size==320&&c->freq_hz==8000&&c->clk_src==1&&c->offset==0);creates++;if(fail==1)return 9;allocated=1;*h=&allocated;return 0;}
static int dac_continuous_register_event_callback(dac_continuous_handle_t h,const dac_event_callbacks_t*c,void*x){assert(h==&allocated&&!x);callbacks=*c;return 0;}
static int dac_continuous_enable(dac_continuous_handle_t h){assert(h==&allocated);if(fail==2)return 9;enabled=1;return 0;}
static int dac_continuous_disable(dac_continuous_handle_t h){assert(h==&allocated&&enabled&&amp==1);enabled=0;return 0;}
static int dac_continuous_del_channels(dac_continuous_handle_t h){assert(h==&allocated&&!enabled&&amp==1);allocated=0;return 0;}
static int dac_continuous_write(dac_continuous_handle_t h,uint8_t*b,size_t n,size_t*done,int timeout){
 assert(h==&allocated&&enabled&&(timeout==100||timeout==200));writes++;*done=short_write?n/2:n;
 if(fail==3){*done=0;return 9;}
 if(timeout==200){assert(amp==1&&n==960);for(unsigned i=0;i<n;i++)assert(b[i]==128);}
 assert(captured_n+*done<=sizeof(captured));memcpy(captured+captured_n,b,*done);captured_n+=*done;
 clock_us+=*done*125;dac_event_data_t ev={.write_bytes=*done*2};callbacks.on_convert_done(h,&ev,NULL);return 0;
}
'''
tail=r'''
int main(void){assert(audio_mode(true)==ESP_ERR_NOT_SUPPORTED&&creates==0);assert(audio_mode(false)==0&&mode==0&&amp==0);int before=creates;assert(audio_mode(false)==0&&creates==before);
assert(dac_clock_rate()==8000&&measure_dac_rate()==8000);captured_n=0;
int16_t pcm[640];for(int i=0;i<640;i++)pcm[i]=cyd_test_tone(i,640);
unsigned samples=state.pcm_samples_written;play(pcm,640);assert(captured_n==640&&state.pcm_samples_written==samples+640);
for(int i=0;i<640;i++)assert(captured[i]==cyd_dac_byte(pcm[i],45));
captured_n=0;state.muted=true;play(pcm,640);play(NULL,640);assert(captured[captured_n-1]==128);
unsigned errors=state.output_errors;short_write=true;play(pcm,160);assert(state.output_errors==errors+1);short_write=false;
unsigned stops=state.dac_dma_stops;callbacks.on_stop(&allocated,NULL,NULL);assert(state.dac_dma_stops==stops+1);
audio_stop();assert(!allocated&&!enabled&&amp==1&&mode==-1);
for(fail=1;fail<=3;fail++){assert(audio_mode(false)!=0&&!allocated&&!enabled&&amp==1);}
fail=0;short_write=true;assert(audio_mode(false)==ESP_ERR_TIMEOUT&&!allocated&&!enabled&&amp==1);short_write=false;
assert(cyd_test_tone(0,16000)==0&&cyd_test_tone(15999,16000)==0);int last=0;for(unsigned i=0;i<16000;i++){int v=cyd_test_tone(i,16000);assert(abs(v)<=1800&&abs(v-last)<=700);last=v;}
for(int v=0;v<=100;v+=5){int last=-1;for(int x=-32768;x<=32767;x++){int b=cyd_dac_byte(x,v);assert(b>=last&&b>=0&&b<=255);last=b;}assert(cyd_dac_byte(0,v)==128);}
assert(cyd_dac_byte(-32768,100)==0&&cyd_dac_byte(32767,100)==255&&cyd_dac_byte(-32768,0)==128);
puts("PASS production DAC transport: mono bytes/PCM identity, midpoint priming, callback rate, mute, short-write/error cleanup, stop counter, sine continuity");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'driver').mkdir();(p/'driver/dac_continuous.h').write_text('');(p/'test.c').write_text(pre+code+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+td,'-I'+str(r/'ui-preview-firmware/main'),str(p/'test.c'),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
