from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'ui-preview-firmware/main/relays.c').read_text()
code=s[s.index('const char*relays_update_name('):s.index('void relays_connection(')]+s[s.index('int relays_order('):]
prefix='''#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#define RELAY_MAX 4
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct{char name[65],host[64],fingerprint[44],call[8];int uid,port;}relay_t;
static relay_t rows[4];static int count,selected,connection_result[4];static time_t connection_time[4];static bool access_verified[4],fail_save;static int saved_selected,saved_count;
static bool persist(void){if(fail_save)return false;saved_selected=selected;saved_count=count;return true;}
typedef relay_t cJSON;
static bool parse(const cJSON*j,relay_t*r){*r=*j;return true;}
static void setup(void){memset(rows,0,sizeof(rows));memset(access_verified,0,sizeof(access_verified));count=3;selected=2;fail_save=false;for(int i=0;i<3;i++){rows[i].uid=i+1;rows[i].port=28883;strcpy(rows[i].host,"example.com");strcpy(rows[i].fingerprint,"fp");}strcpy(rows[0].name,"ordinary");strcpy(rows[1].name,"西南集群（测试）");strcpy(rows[2].name,"verified");access_verified[2]=true;}
'''
tail='''int main(void){setup();relay_t rename=rows[0];strcpy(rename.name,"new name");strcpy(rename.fingerprint,"wrong");assert(relays_update_name(&rename));assert(!strcmp(rows[0].name,"ordinary"));strcpy(rename.fingerprint,"fp");fail_save=true;assert(relays_update_name(&rename));assert(!strcmp(rows[0].name,"ordinary"));fail_save=false;assert(!relays_update_name(&rename));assert(!strcmp(rows[0].name,"new name"));setup();int order[4];assert(relays_order(order)==3&&order[0]==2&&order[1]==0&&order[2]==1);relay_t target=rows[0];assert(relays_remove(2,&rows[2]));assert(count==3);relay_t wrong=target;wrong.uid=99;assert(relays_remove(0,&wrong));assert(count==3);fail_save=true;assert(relays_remove(0,&target));assert(count==3&&selected==2&&rows[0].uid==1&&access_verified[2]);fail_save=false;assert(!relays_remove(0,&target));assert(count==2&&selected==1&&rows[selected].uid==3&&access_verified[1]);assert(saved_selected==1&&saved_count==2);assert(relays_remove(0,&target));assert(!relays_remove(0,&rows[0]));assert(count==1&&selected==0&&rows[0].uid==3);assert(relays_remove(0,&rows[0]));return 0;}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+code+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
print('PASS relay deletion, stale target, current protection, persistence failure rollback, priority order')
