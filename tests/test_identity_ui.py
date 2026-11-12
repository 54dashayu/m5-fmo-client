"""Run the actual device identity screen's key and paging logic on the host."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'ui-preview-firmware/main/main.c').read_text()
key=source.split(' if(page==IDENTITY){',1)[1].split(' if(page==VOLUME)',1)[0]
key='if(page==IDENTITY){'+key
render=source.split(' }else if(page==CA_LIST){',1)[1].split(' }else if(page==CA_CONFIRM)',1)[0]
pre=r'''
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
enum{IDENTITY,CA_LIST,CA_CONFIRM,WEB};
typedef struct{int id;char name[97];bool current,has_certificate,active;}device_profile_t;
static device_profile_t ca_profiles[10],ca_target;static int ca_count,page,selection,keys,loads,requests,last_id;static bool ca_submitted;static char ca_notice[96];
static void go(int next){page=next;selection=0;ca_submitted=false;}
static void load_ca_profiles(void){loads++;}
static bool device_profile_switch(int id){requests++;last_id=id;return true;}
static void buttons_block(int*k){(void)k;}
static char labels[7][256],*title=labels[5],*footer=labels[6],*lines[5]={labels[0],labels[1],labels[2],labels[3],labels[4]};
#define lv_color_hex(x) (x)
static void lv_obj_set_style_text_color(char*p,unsigned c,int i){(void)p;(void)c;(void)i;}
static void lv_label_set_text(char*p,const char*s){snprintf(p,256,"%s",s);}
static void lv_label_set_text_fmt(char*p,const char*fmt,...){va_list a;va_start(a,fmt);vsnprintf(p,256,fmt,a);va_end(a);}
'''
tail=r'''
int main(void){for(int i=0;i<6;i++){ca_profiles[i].id=i+1;snprintf(ca_profiles[i].name,97,"CA%d",i+1);ca_profiles[i].has_certificate=true;}ca_profiles[0].current=true;ca_profiles[0].active=true;ca_profiles[2].has_certificate=false;ca_count=6;
page=IDENTITY;key(1);assert(page==CA_LIST);key(1);assert(!requests&&page==CA_LIST);key(2);key(2);key(1);assert(!requests&&page==CA_CONFIRM&&ca_target.id==3);key(0);selection=1;key(1);assert(page==CA_CONFIRM&&ca_target.id==2);key(0);assert(page==CA_LIST&&!requests);selection=1;key(1);key(1);assert(requests==1&&last_id==2&&ca_submitted);key(1);key(0);assert(requests==1&&page==CA_CONFIRM);
go(CA_LIST);selection=4;render();assert(strstr(title,"2/2")&&strstr(lines[0],"> CA5")&&!lines[2][0]);selection=5;key(2);assert(selection==0);key(0);assert(selection==5);ca_count=0;key(1);assert(loads==1);puts("PASS identity UI entry, root-only confirmation, confirmation/cancel, single submit and pagination");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(pre+'static void key(int k){'+key+'}\nstatic void render(void){'+render+'}\n'+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'t.c'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
# Switching must use the existing worker/transaction, and reboot queues discovery.
d=(root/'ui-preview-firmware/main/device_web.c').read_text()
a=d.split('bool device_profile_switch(int id){',1)[1].split('static void profile_switch_tick',1)[0]
assert 'nvs_set_' not in a and 'atomic_compare_exchange' in a
assert 'profile_switch_tick();enrollment_tick();' in d
assert 'refresh_identity){discovery_identity_changed();}' in d
print('PASS UI queues worker operation; boot invalidates old proof and refreshes discovery')
