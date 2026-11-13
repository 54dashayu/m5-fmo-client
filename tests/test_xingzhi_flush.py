"""Exercise production S3 flush with non-DMA source pixels and deferred completion."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/components/lv_porting/xingzhi_display.c').read_text()
production=s[s.index('static bool done('):s.index('void disp_driver_set_brightness(')]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
typedef uint16_t lv_color_t;
typedef struct {int x1,y1,x2,y2;} lv_area_t;
typedef struct {unsigned ready;} lv_disp_drv_t;
typedef void* esp_lcd_panel_io_handle_t;
typedef int esp_lcd_panel_io_event_data_t;
typedef int esp_err_t;
#define ESP_OK 0
#define DISPLAY_PIXELS (240*40)
static lv_color_t storage[DISPLAY_PIXELS],*dma_pixels=storage;
static atomic_uint ui_flush_sent,ui_flush_done,ui_flush_errors,ui_flush_non_dma,ui_flush_last_error;
static void *panel;static bool update=true;static int error;static const void *queued;
static void lv_disp_flush_ready(lv_disp_drv_t*d){d->ready++;}
static bool esp_ptr_dma_capable(const void*p){return p==storage;}
static int esp_lcd_panel_draw_bitmap(void*p,int x1,int y1,int x2,int y2,const void*data){(void)p;assert(x2>x1&&y2>y1);assert(data==storage);queued=data;return error;}
'''
post=r'''
int main(void){lv_disp_drv_t d={0};lv_area_t a={0,0,239,39};lv_color_t *external=malloc(sizeof(storage));assert(external);for(unsigned i=0;i<DISPLAY_PIXELS;i++)external[i]=(uint16_t)i;
flush(&d,&a,external);assert(ui_flush_sent==1&&ui_flush_done==0&&d.ready==0);assert(!memcmp(queued,external,sizeof(storage)));memset(external,0,sizeof(storage));assert(storage[1]==1&&storage[9599]==9599);done(NULL,NULL,&d);assert(ui_flush_done==1&&d.ready==1);
a=(lv_area_t){17,4,19,4};external[0]=1;external[1]=2;external[2]=3;flush(&d,&a,external);assert(storage[0]==1&&storage[2]==3);done(NULL,NULL,&d);assert(ui_flush_sent==2&&ui_flush_done==2);
error=0x101;flush(&d,&a,external);assert(ui_flush_errors==1&&ui_flush_last_error==0x101&&d.ready==3);update=false;flush(&d,&a,external);assert(d.ready==4&&ui_flush_sent==3);free(external);puts("PASS production S3 DMA staging: full/odd regions, external source lifetime, deferred completion and error release");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.c').write_text(pre+production+post)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
