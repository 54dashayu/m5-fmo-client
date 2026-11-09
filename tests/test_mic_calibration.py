from pathlib import Path
import tempfile,subprocess
s=(Path(__file__).resolve().parents[1]/'ui-preview-firmware/main/voice.c').read_text()
code=s[s.index('static int measure_mic_rate('):s.index('static void tx_status(')]
prefix=r"""
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#define I2S_NUM_0 0
#define ESP_OK 0
#define pdMS_TO_TICKS(x) (x)
static int mic_clock_request=8000, broken=0, halted=0;
static int64_t now=0;
static int64_t esp_timer_get_time(void){return now;}
static int audio_mode(bool mic){assert(mic);return 0;}
static void audio_stop(void){halted++;}
static int i2s_read(int port,void *buf,size_t n,size_t *got,int timeout){(void)port;(void)buf;(void)timeout;*got=broken?0:n;now+=(int64_t)(n/2)*1000000*6/(mic_clock_request*5);return 0;}
"""
main=r"""
int main(void){int initial=0;int measured=calibrate_mic_rate(&initial);assert(initial>6600&&initial<6700);assert(measured>=7920&&measured<=8080);assert(mic_clock_request>9500&&mic_clock_request<9700);assert(halted==2);broken=1;assert(calibrate_mic_rate(&initial)==0);return 0;}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+code+main)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
print('PASS actual calibration corrects slow DMA clock, bounds retries and rejects short reads')
