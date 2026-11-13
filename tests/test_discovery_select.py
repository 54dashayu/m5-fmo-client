"""Run actual discovery selection before verification, with stale/revision gates."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/discovery.c').read_text()
fn=s[s.index('int discovery_list('):s.index('// Save a fresh')]
pre='''#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "cJSON.h"
#define DISCOVERY_MAX 8
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
#define atomic_load(p) (*(p))
typedef struct{char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;}relay_t;
typedef struct{relay_t relay;time_t verified,attempted,seen;bool mutual;}candidate_t;
static candidate_t rows[8];static int count=1,mutex=1;static uint32_t revision=7;static bool owner_valid=true,busy;static char owner[65]="owner";static int saved,selected=-1;static relay_t stored;
static bool fresh(candidate_t*c,time_t now){return now>=c->seen&&now-c->seen<1200;}
static bool voice_busy(void){return busy;}
static bool current_owner(char*out){strcpy(out,"owner");return owner_valid;}
static bool same(const relay_t*a,const relay_t*b){return a->uid==b->uid&&!strcmp(a->fingerprint,b->fingerprint);}
static int relays_count(void){return saved;}
static bool relays_get(int i,relay_t*out){if(i>=saved)return false;*out=stored;return true;}
static bool device_select_relay(int i){selected=i;return true;}
static const char*relays_add(cJSON*j){stored=rows[0].relay;saved=1;assert(cJSON_GetObjectItem(j,"uid")->valueint==1845);return NULL;}
'''
tail='''int main(void){rows[0].relay=(relay_t){.uid=1845,.name="Test",.call="BI4TES",.host="example.net",.port=1883};rows[0].seen=time(NULL);assert(rows[0].verified==0);assert(!discovery_select(0,7));assert(saved==1&&selected==0&&rows[0].verified==0);selected=-1;assert(discovery_select(0,6));assert(selected==-1);busy=true;assert(discovery_select(0,7));busy=false;rows[0].seen-=1200;assert(discovery_select(0,7));rows[0].seen=time(NULL);owner_valid=false;assert(discovery_select(0,7));assert(selected==-1);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';p.write_text(pre+fn+tail);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cj),str(p),str(cj/'cJSON.c'),'-o',d+'/t'],check=True);subprocess.run([d+'/t'],check=True)
print('PASS unverified candidate selected without claiming verified; stale, changed revision, busy and identity gates')
