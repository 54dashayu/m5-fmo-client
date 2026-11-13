from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/discovery.c').read_text();fn=s[s.index('const char*discovery_add('):s.index('static const char*candidate_status(')]
pre=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "cJSON.h"
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct {char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;}relay_t;
typedef struct {relay_t relay;time_t verified,attempted,seen;bool mutual;}candidate_t;
static candidate_t rows[8];static int count=1,adds=0;static uint32_t revision=9;static time_t updated;static bool busy;static const char*save_error;
static bool voice_busy(void){return busy;}
static const char*relays_add(cJSON*j){adds++;assert(cJSON_GetObjectItemCaseSensitive(j,"uid")->valueint==1845);assert(!strcmp(cJSON_GetObjectItemCaseSensitive(j,"host")->valuestring,"example.net"));assert(!cJSON_GetObjectItemCaseSensitive(j,"accessVerified"));return save_error;}
'''
tail=r'''
int main(void){time_t now=time(NULL);updated=rows[0].seen=now;rows[0].relay=(relay_t){.uid=1845,.name="test",.call="BI4TES",.host="example.net",.port=1883};assert(!discovery_add(0,9));assert(adds==1);assert(rows[0].verified==0);assert(discovery_add(0,8));assert(discovery_add(-1,9));assert(discovery_add(1,9));busy=true;assert(discovery_add(0,9));busy=false;updated=now-600;assert(discovery_add(0,9));updated=now;rows[0].seen=now-1200;assert(discovery_add(0,9));assert(adds==1);rows[0].seen=now;save_error="full";assert(!strcmp(discovery_add(0,9),"full"));return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.c';p.write_text(pre+fn+tail);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cj),str(p),str(cj/'cJSON.c'),'-lm','-o',td+'/t'],check=True);subprocess.run([td+'/t'],check=True)
print('PASS manual add: fresh unverified accepted without authentication upgrade, stale/revision/index/busy rejected, save failure preserved')
