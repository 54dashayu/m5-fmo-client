"""Execute the actual added UI key branches against all logical image actions."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/main.c').read_text()
code=s[s.index(' if(page==MEDIA){'):s.index(' if(page==MENU){')]
pre=r'''
#include "sstv_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
enum{HOME,MEDIA,RELAYS,SSTV,GALLERY,RESOURCES,MENU,SSTV_TX_CONFIRM,SSTV_TX_PAGE};
static int page,selection,gallery_count,loads,saves;static unsigned sstv_lines;static uint32_t sstv_saved;
static sstv_entry_t gallery_entries[SSTV_GALLERY_MAX];
int sstv_store_newer(sstv_entry_t *e,uint32_t after){(void)e;return after?0:3;}
int sstv_store_page(sstv_entry_t *e,uint32_t before){(void)e;(void)before;return 0;}
int sstv_store_list(sstv_entry_t *e){(void)e;return 3;}
static bool busy;
static unsigned gallery_revision;
unsigned sstv_store_revision(void){return gallery_revision;}
static uint32_t sstv_last_saved;
static uint32_t tx_image_id;static bool tx_launch_failed;
static unsigned tx_starts;static uint32_t tx_last_id;
static bool voice_sstv_start(uint32_t id){tx_starts++;tx_last_id=id;return id&&!busy;}
static void voice_sstv_stop(void){}
static bool save_ok=true;static sstv_status_t sstv_status;static char image_notice[112];
static void go(int n){page=n;selection=0;if(n==SSTV){sstv_saved=0;sstv_status=SSTV_WAITING;image_notice[0]=0;}}
static bool gallery_preview_ready=true;
#ifdef FMO_SSTV_STREAMING
static void gallery_request(int direction){(void)direction;loads++;}
#else
static void gallery_load(void){loads++;}
#endif
static bool voice_busy(void){return busy;}
static bool sstv_rx_save(uint32_t*i){saves++;if(save_ok)*i=42;return save_ok;}
static bool sstv_rx_save_partial(uint32_t*i){saves++;*i=43;return true;}
#ifdef CONFIG_FMO_BOARD_XINGZHI
static void sstv_save_begin(void){if(sstv_status==SSTV_INTERRUPTED?sstv_rx_save_partial(&sstv_saved):sstv_rx_save(&sstv_saved)){uint32_t id=sstv_saved;go(SSTV);sstv_last_saved=id;}}
#endif
const char*sstv_store_error(void){return NULL;}
const char*sstv_store_save_error(void){return "保存失败：相册已满或存储忙";}
'''
tail=r'''
int main(void){page=MEDIA;selection=0;key(0);assert(selection==3);key(2);assert(selection==0);key(2);key(1);assert(page==SSTV);
sstv_status=SSTV_WAITING;key(1);assert(!saves);sstv_status=SSTV_RECEIVING;key(1);key(2);assert(!saves&&page==SSTV);key(0);assert(page==MEDIA);
page=SSTV;sstv_status=SSTV_COMPLETE;busy=true;key(1);assert(!saves);busy=false;key(1);
#ifdef FMO_FULL_SSTV
assert(saves==1&&sstv_saved==0&&sstv_last_saved==42&&sstv_status==SSTV_WAITING&&page==SSTV);key(1);assert(saves==1);
sstv_status=SSTV_COMPLETE;save_ok=false;key(1);assert(saves==2&&sstv_status==SSTV_COMPLETE&&!sstv_saved);save_ok=true;key(1);assert(saves==3&&sstv_status==SSTV_WAITING&&!sstv_saved);key(0);assert(page==MEDIA);
#else
assert(saves==1&&sstv_saved==42);key(1);assert(saves==1);
#endif

page=SSTV;sstv_status=SSTV_INTERRUPTED;sstv_lines=20;sstv_saved=0;busy=true;int before=saves;key(1);assert(saves==before);busy=false;key(1);assert(saves==before+1);
#ifdef FMO_SSTV_STREAMING
page=GALLERY;gallery_count=3;selection=0;key(0);key(2);assert(selection==0&&loads==2);gallery_preview_ready=false;key(1);assert(page==GALLERY);gallery_preview_ready=true;key(1);assert(page==SSTV_TX_CONFIRM);key(0);assert(page==GALLERY);gallery_count=0;key(1);assert(page==MEDIA);
#else
page=GALLERY;gallery_count=3;selection=0;key(0);assert(selection==2);key(2);assert(selection==0&&loads==2);key(1);assert(page==SSTV_TX_CONFIRM);key(0);assert(page==GALLERY);
page=GALLERY;gallery_count=0;key(0);key(2);assert(loads==2);key(1);assert(page==MEDIA);
#endif
page=GALLERY;gallery_count=1;selection=0;gallery_entries[0].id=42;busy=false;key(1);assert(page==SSTV_TX_CONFIRM&&tx_starts==0);key(1);assert(page==SSTV_TX_PAGE&&tx_starts==1&&tx_last_id==42&&!tx_launch_failed);key(0);assert(page==GALLERY);busy=true;gallery_count=1;key(1);key(1);assert(page==SSTV_TX_PAGE&&tx_launch_failed&&tx_starts==2);busy=false;
(void)sstv_last_saved;puts("PASS image UI: entry selection, exit while receiving, manual interrupted save, busy/duplicate save guard, gallery wrap and empty state");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'ui.c';p.write_text(pre+'static void key(int k){'+code+'}\n'+tail)
 for flags in ([],['-DCONFIG_FMO_BOARD_XINGZHI'],['-DCONFIG_IDF_TARGET_ESP32']):
  subprocess.run(['cc',*flags,'-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
