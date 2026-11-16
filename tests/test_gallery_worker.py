"""Production M5 gallery worker: queueing, small preview, failures, and no decoder init."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text()
code=s[s.index('#include "sstv_m5_workspace.h"'):s.index('static void image_draw(')]
pre=r'''
#include "sstv_store.h"
#include "sstv_m5_workspace.h"
sstv_m5_workspace_t sstv_m5_workspace;
atomic_int sstv_m5_workspace_owner;
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static char image_notice[112];static bool gallery_space_valid,gallery_preview_ready,busy,read_ok=true;
static int gallery_count,selection,list_calls,preview_calls;
static sstv_entry_t gallery_entries[SSTV_GALLERY_MAX];
static bool voice_busy(void){return busy;}
int sstv_store_list(sstv_entry_t *e){list_calls++;e[0].id=10;e[1].id=9;return 2;}
int sstv_store_page(sstv_entry_t *e,uint32_t before){(void)e;(void)before;return 0;}
int sstv_store_newer(sstv_entry_t *e,uint32_t after){(void)after;return sstv_store_list(e);}
uint32_t sstv_store_neighbor(uint32_t id,bool newer){(void)newer;return id==10?9:10;}
bool sstv_store_thumbnail(uint32_t id,uint8_t*p,unsigned w,unsigned h){assert(id);assert((w==80&&h==60)||(w==40&&h==30));preview_calls++;memset(p,42,w*h);return read_ok;}
'''
code='#define calloc(...) forbidden_gallery_heap_allocation\n#define malloc(...) forbidden_gallery_heap_allocation\n'+code+'\n#undef calloc\n#undef malloc\n'
post=r'''
int main(void){
assert(sstv_workspace_claim(SSTV_WORKSPACE_RX));memset(&sstv_m5_workspace,0xa5,sizeof(sstv_m5_workspace));gallery_request(0);ui_gallery_worker_tick();assert(!gallery_preview_ready&&!preview_calls&&!list_calls);for(unsigned i=0;i<sizeof(sstv_m5_workspace);i++)assert(((unsigned char*)&sstv_m5_workspace)[i]==0xa5);sstv_workspace_release(SSTV_WORKSPACE_RX);atomic_store(&gallery_work,0);
gallery_request(0);assert(atomic_load(&gallery_work)==1&&!list_calls&&!preview_calls);gallery_request(1);assert(gallery_direction==0);ui_gallery_worker_tick();assert(atomic_load(&gallery_work)==3&&list_calls==1&&preview_calls==2&&gallery_preview_ready&&gallery_pixels[0]==42);ui_gallery_worker_tick();assert(preview_calls==2);
atomic_store(&gallery_work,0);gallery_request(1);assert(!gallery_preview_ready);ui_gallery_worker_tick();assert(selection==1&&gallery_preview_ready);
atomic_store(&gallery_work,0);read_ok=false;gallery_request(-1);ui_gallery_worker_tick();assert(!gallery_preview_ready&&image_notice[0]);
atomic_store(&gallery_work,0);busy=true;int old=preview_calls;gallery_request(0);ui_gallery_worker_tick();assert(preview_calls==old&&!gallery_preview_ready&&image_notice[0]);busy=false;read_ok=true;for(int i=0;i<1000;i++){atomic_store(&gallery_work,0);gallery_request(0);ui_gallery_worker_tick();assert(gallery_preview_ready&&gallery_pixels[4799]==42);}assert(sizeof(gallery_pixels)+sizeof(gallery_sides)==7200);puts("PASS asynchronous M5 gallery: no I/O on request, duplicate suppression, reserved 7200-byte three-image thumbnails with no heap allocation, repeated entry, paging and failure gating");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+code+post)
 subprocess.run(['cc','-DCONFIG_IDF_TARGET_ESP32','-std=c11','-Wall','-Wextra','-Werror','-I'+str(r/'ui-preview-firmware/main'),str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
