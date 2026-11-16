"""Actual PSRAM gallery: three images, clipped sides and selected source identity."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text();code=s[s.index('#ifndef FMO_SSTV_STREAMING\n#ifdef FMO_FULL_SSTV'):s.index('static device_profile_t ca_profiles')]
pre=r'''
#include "sstv_store.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint16_t lv_color_t;
static uint16_t frame[240*180+1],*image_display=frame;
static uint8_t source[153600];static bool gallery_preview_ready,gallery_space_valid;static sstv_storage_t gallery_space;static char image_notice[112];static int gallery_count=3,selection=1;static unsigned calls[4],count;
static sstv_entry_t gallery_entries[3]={{.id=1},{.id=2},{.id=3}};
static uint16_t lv_color_make(unsigned r,unsigned g,unsigned b){return (r>>3)<<11|(g>>2)<<5|(b>>3);}
static uint8_t *sstv_rx_pixels(void){return source;}
bool sstv_store_stats(sstv_storage_t *p){(void)p;return true;}
const char *sstv_store_error(void){return NULL;}
bool sstv_store_load(uint32_t id,uint8_t *p){assert(count<4);calls[count++]=id;unsigned c=id==1?0xf800:id==2?0x07e0:0x001f;for(unsigned i=0;i<76800;i++){p[i*2]=c;p[i*2+1]=c>>8;}return true;}
static void image_changed(void){}
#define malloc(...) forbidden_allocation
#define calloc(...) forbidden_allocation
'''
tail=r'''
int main(void){frame[240*180]=0xabcd;gallery_load();assert(gallery_preview_ready&&count==3&&calls[0]==1&&calls[1]==3&&calls[2]==2);assert(frame[90*240+120]==0x07e0&&frame[90*240]==0xf800&&frame[90*240+239]==0x001f);assert(frame[43*240+58]==lv_color_make(88,214,178)&&frame[240*180]==0xabcd);assert(source[0]==0xe0&&source[1]==0x07);gallery_count=1;selection=0;count=0;gallery_load();assert(count==1&&frame[90*240]==lv_color_make(13,21,29));gallery_count=0;count=0;gallery_load();assert(!gallery_preview_ready&&!count);puts("PASS Xiaozhi three-image gallery: clipped sides, 120x90 focus, selected image, singleton/empty and no extra allocation");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+code+tail);subprocess.run(['cc','-DFMO_FULL_SSTV','-Wall','-Wextra','-Werror','-I'+str(r/'ui-preview-firmware/main'),str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
