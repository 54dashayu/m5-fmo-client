from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/side_leds.c').read_text()
assert 'gpio_set_direction(' not in s and 'gpio_ll_od_enable' not in s
s='\n'.join(x for x in s.splitlines() if not x.startswith('#include'))
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "side_leds.h"
#define ESP_OK 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define LED_MODEL_WS2812 1
#define LED_STRIP_COLOR_COMPONENT_FMT_GRB 2
#define RMT_CLK_SRC_DEFAULT 0
typedef int esp_err_t;
typedef void*led_strip_handle_t;
typedef struct {int strip_gpio_num,max_leds,led_model,color_component_format;struct {bool invert_out;}flags;}led_strip_config_t;
typedef struct {int clk_src,resolution_hz,mem_block_symbols;struct {bool with_dma;}flags;}led_strip_rmt_config_t;
static int red,green,writes,fail;
static int led_strip_new_rmt_device(const led_strip_config_t*c,const led_strip_rmt_config_t*r,led_strip_handle_t*out){
 assert(c->strip_gpio_num==15&&c->max_leds==10&&c->color_component_format==2&&!c->flags.invert_out);
 assert(r->resolution_hz==10000000&&r->mem_block_symbols>=241&&!r->flags.with_dma);*out=&writes;return 0;
}
static int led_strip_set_pixel(led_strip_handle_t s,int i,int r,int g,int b){assert(s&&i<10&&r<=24&&g<=24&&b==0);red=r;green=g;return 0;}
static int led_strip_refresh(led_strip_handle_t s){assert(s);writes++;return fail;}
'''
main=r'''
int main(void){
 side_leds_init();assert(red==0&&green==0);
 side_leds_update(false,true,100);assert(green==1&&red==0);
 side_leds_update(false,true,500);assert(green>=12&&green<=13&&red==0);
 side_leds_update(false,true,900);assert(green==24&&red==0);
 side_leds_update(true,true,1000);assert(red==1&&green==0);
 int n=writes;fail=7;side_leds_update(false,false,1010);
 side_leds_view_t v;side_leds_view(&v);assert(v.ready&&v.failures==1&&v.error==7&&v.color==2);
 fail=0;side_leds_update(false,false,1020);side_leds_view(&v);assert(v.color==0&&v.error==0&&writes==n+2);
 side_leds_update(false,false,1030);assert(writes==n+2);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+s+main)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
print('PASS reference GRB backend, low brightness, smooth 1600ms breathe, TX priority, errors and idle off')
