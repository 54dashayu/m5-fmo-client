"""Exercise actual Plus touch routing with deterministic UI/storage stubs."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text()
actual=s[s.index('static int cyd_content_rows(void)'):s.index('static void cyd_controls(void)')]
enums=s[s.index('enum {HOME,'):s.index(';',s.index('enum {HOME,'))+1]
pre='''#include "cyd28_plus_touch.h"
#include <stdatomic.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
'''+enums+'''
static int page,selection,discovered_count=6,gallery_count=3,gallery_direction,confirmed=-99,invalidated;
static int volume;
static void voice_volume(int d){volume+=d;}
static unsigned gallery_revision=1,revision=1;
static bool gallery_zoom,gallery_preview_ready=true,gallery_side_ready[2]={true,true};
static atomic_int gallery_work;
const char *items[10];
#define KEY_SHORT 1
static int relays_count(void){return 6;}
static int relays_order_at(int pos){return 5-pos;}
static int relays_order_position(int index){return index<0?6:5-index;}
static unsigned sstv_store_revision(void){return revision;}
static void go(int next){page=next;selection=0;gallery_zoom=false;}
static void image_changed(void){invalidated++;}
static void gallery_request(int direction){gallery_direction=direction;}
static void key(int k,int action,void*ctx){(void)ctx;assert(k>=0&&k<=2&&action==KEY_SHORT);confirmed=selection;}
'''
post='''
int main(void){
 page=HOME;cyd_activate(1);assert(volume==5);cyd_activate(2);assert(volume==0);cyd_activate(0);assert(page==MENU);
 page=MENU;selection=0;assert(cyd_content_rows()==5);cyd_activate(12);assert(confirmed==2);
 cyd_activate(2);assert(selection==5);cyd_activate(14);assert(confirmed==9);cyd_activate(2);assert(selection==0);cyd_activate(0);assert(selection==5);
 page=RELAYS;selection=5;confirmed=-99;cyd_activate(12);assert(selection==3&&confirmed==3);cyd_activate(2);assert(selection==1);assert(cyd_content_rows()==3);cyd_activate(12);assert(selection==-1&&confirmed==-1);confirmed=-99;cyd_activate(13);assert(confirmed==-99);
 page=DISCOVER;selection=4;assert(cyd_content_rows()==3);cyd_activate(12);assert(confirmed==6);cyd_activate(2);assert(selection==0);
 page=GALLERY;selection=0;assert(cyd_content_kind()==CYD_CONTENT_GALLERY);cyd_activate(20);assert(gallery_direction==-1);cyd_activate(22);assert(gallery_direction==1);gallery_side_ready[0]=false;gallery_direction=0;cyd_activate(20);assert(gallery_direction==0);
 cyd_activate(21);assert(gallery_zoom&&invalidated==1&&cyd_content_kind()==CYD_CONTENT_ZOOM);cyd_activate(3);assert(page==GALLERY&&!gallery_zoom);cyd_activate(3);assert(page==HOME);
 page=GALLERY;atomic_store(&gallery_work,1);cyd_activate(21);cyd_activate(3);assert(page==GALLERY&&!gallery_zoom);atomic_store(&gallery_work,0);gallery_count=0;cyd_activate(21);assert(!gallery_zoom);
 gallery_count=3;revision=2;cyd_activate(21);assert(!gallery_zoom&&selection==0);
 puts("PASS actual direct touch routing: menu, ordered relay/discovery paging, blank rows, gallery select/zoom/back/busy/stale guards");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'direct.c';p.write_text(pre+actual+post);exe=str(Path(td)/'direct')
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(r/'ui-preview-firmware/components/lv_porting'),str(p),'-o',exe],check=True);subprocess.run([exe],check=True)
