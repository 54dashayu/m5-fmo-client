from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/main.c').read_text()
key=s.split(' if(page==RELAYS){',1)[1].split('\n if(page==DISCOVER)',1)[0];key=key.rsplit('}',1)[0]
render=s.split(' }else if(page==RELAYS){',1)[1].split(' }else if(page==DISCOVER)',1)[0]
pre=r'''
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>
#define RELAY_MAX 8
#define HOME 1
#define DISCOVER 2
static int selection,current,count=8,chosen=-1,destination;
static int sorted[8]={6,0,1,2,3,4,5,7};
typedef struct{char name[65],call[8];}relay_t;
static const char*relays_profile_name(void){return "BH1JSS M5";}
static int relays_count(void){return count;}
static int relays_order_at(int p){return p>=0&&p<count?sorted[p]:-1;}
static int relays_order_position(int index){for(int i=0;i<count;i++)if(sorted[i]==index)return i;return count;}
static bool relays_get(int i,relay_t*r){snprintf(r->name,65,"node%d",i);strcpy(r->call,"BH1JSS");return true;}
static void go(int n){destination=n;}
static bool device_select_relay(int i){chosen=i;return true;}
static char labels[7][256];static char*title=labels[5],*footer=labels[6],*lines[5]={labels[0],labels[1],labels[2],labels[3],labels[4]};
static unsigned colors[7];
#define lv_color_hex(x) (x)
static void lv_obj_set_style_text_color(char*p,unsigned color,int ignored){(void)ignored;colors[(p-labels[0])/256]=color;}
static void lv_label_set_text(char*p,const char*s){snprintf(p,256,"%s",s);}
static void lv_label_set_text_fmt(char*p,const char*fmt,...){va_list a;va_start(a,fmt);vsnprintf(p,256,fmt,a);va_end(a);}
#define relay_glyph 0
static void name_for_display(char*out,size_t n,const char*s,int ignored){(void)ignored;snprintf(out,n,"%s",s);}
'''
tail=r'''
int main(void){selection=6;current=6;render();assert(strstr(title,"1/3"));assert(strstr(lines[0],"> [当前] node6"));for(int i=0;i<4;i++)key(2);assert(selection==3);render();assert(strstr(title,"2/3"));assert(strstr(lines[0],"> node3"));key(1);assert(chosen==3&&destination==HOME);for(int i=0;i<4;i++)key(2);assert(selection==-1);render();assert(strstr(title,"3/3"));assert(strstr(lines[0],"+ 发现中继"));assert(colors[0]==0xffc567);assert(!lines[1][0]);key(1);assert(destination==DISCOVER);key(2);assert(selection==6);render();assert(colors[0]==0xecf2f8);key(0);assert(selection==-1);count=4;selection=2;render();assert(strstr(title,"1/2"));key(2);render();assert(selection==-1&&strstr(title,"2/2"));assert(!lines[1][0]);return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.c';p.write_text(pre+'static void key(int k){'+key+'}\nstatic void render(void){'+render+'}\n'+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/t'],check=True);subprocess.run([td+'/t'],check=True)
print('PASS actual relay UI: 4/8 entries, priority ordering, page boundaries, B selects correct relay, discover entry, wraparound and cleared empty rows')
