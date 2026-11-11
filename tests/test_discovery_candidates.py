from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/discovery.c').read_text()
usable=s[s.index('static bool usable('):s.index('static bool parse(')]
funcs=s[s.index('static const char*candidate_status('):]
pre='''#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "cJSON.h"
#define DISCOVERY_MAX 8
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
#define atomic_load(p) (*(p))
typedef struct {char name[65],call[8];uint32_t uid;}relay_t;
typedef struct {relay_t relay;time_t verified,attempted,seen;}candidate_t;
static candidate_t rows[8];static int count;static uint32_t revision=7,checking_uid;
static time_t updated;static bool owner_valid=true;static char message[96]="test";
'''
tail='''int main(void){time_t now=time(NULL);updated=now;count=4;for(int i=0;i<4;i++){rows[i].relay.uid=i+1;strcpy(rows[i].relay.name,"node");strcpy(rows[i].relay.call,"BH1JSS");rows[i].seen=now;}rows[0].attempted=now;rows[1].verified=now;rows[2].verified=now;checking_uid=4;
cJSON*j=discovery_json(),*a=cJSON_GetObjectItemCaseSensitive(j,"candidateServers");assert(cJSON_GetArraySize(a)==4);assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"servers"))==2);
for(int i=0;i<2;i++){cJSON*v=cJSON_GetArrayItem(a,i);assert(cJSON_GetObjectItemCaseSensitive(v,"uid")->valueint==i+2);assert(cJSON_GetObjectItemCaseSensitive(v,"selectIndex")->valueint==i);assert(!strcmp(cJSON_GetObjectItemCaseSensitive(v,"state")->valuestring,"verified"));}
assert(!strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(a,2),"state")->valuestring,"checking"));assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(a,3),"selectIndex")->valueint==-1);cJSON_Delete(j);
owner_valid=false;j=discovery_json();assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"servers"))==0);cJSON_Delete(j);owner_valid=true;updated=now-601;j=discovery_json();a=cJSON_GetObjectItemCaseSensitive(j,"candidateServers");for(int i=0;i<4;i++){assert(!strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(a,i),"state")->valuestring,"stale"));assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(a,i),"selectIndex")->valueint==-1);}cJSON_Delete(j);
count=0;j=discovery_json();assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"candidateServers"))==0);cJSON_Delete(j);
count=1;updated=now;rows[0].relay.uid=1845;rows[0].seen=now;rows[0].verified=rows[0].attempted=0;j=discovery_json();a=cJSON_GetObjectItemCaseSensitive(j,"candidateServers");assert(cJSON_GetArraySize(a)==1);cJSON*v=cJSON_GetArrayItem(a,0);assert(!cJSON_GetObjectItemCaseSensitive(v,"adminConfirmed"));assert(!strcmp(cJSON_GetObjectItemCaseSensitive(v,"state")->valuestring,"pending"));assert(cJSON_GetObjectItemCaseSensitive(v,"selectIndex")->valueint==-1);cJSON_Delete(j);
rows[0].verified=now;j=discovery_json();v=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(j,"candidateServers"),0);assert(!strcmp(cJSON_GetObjectItemCaseSensitive(v,"state")->valuestring,"verified"));assert(cJSON_GetObjectItemCaseSensitive(v,"selectIndex")->valueint==0);cJSON_Delete(j);
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.c';p.write_text(pre+usable+funcs+tail);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cj),str(p),str(cj/'cJSON.c'),'-o',td+'/t'],check=True);subprocess.run([td+'/t'],check=True)
print('PASS candidate list shows all states, verified-first ordering, original selection indexes, identity and expiry gates')
