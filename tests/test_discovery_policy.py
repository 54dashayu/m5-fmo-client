from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/discovery.c').read_text()
functions=s[s.index('static const char *str'):s.index('static void status')]+s[s.index('static bool fresh'):s.index('static bool fetch_catalog')]
prefix=r'''
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include "cJSON.h"
#include "utf8_name.h"
typedef struct {char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;} relay_t;
typedef struct {relay_t relay;time_t verified,attempted,seen;bool mutual;} candidate_t;
static time_t updated=1000;
static bool wire_callsign(const char*c){return c&&strlen(c)>=3&&strlen(c)<=6;}
static bool wire_unb64(const char*s,uint8_t*b,int n){(void)b;(void)n;return s&&strlen(s)==43;}
'''
tail=r'''
int main(void){cJSON*j=cJSON_Parse("{\"name\":\"西南集群\",\"callsign\":\"BH1JSS\",\"host\":\"example.net\",\"uid\":1791,\"port\":28883,\"fingerprint\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"lastSeen\":1000,\"sourceVerified\":true,\"rootTrusted\":false}");candidate_t c;assert(!parse(j,&c,1000));cJSON_ReplaceItemInObject(j,"rootTrusted",cJSON_CreateTrue());assert(parse(j,&c,1000));assert(!usable(&c,1000));c.verified=1000;assert(usable(&c,1001));assert(!usable(&c,999));assert(!usable(&c,1600));assert(!parse(j,&c,999));assert(!parse(j,&c,2200));assert(parse(j,&c,1000));relay_t r=c.relay;assert(same(&r,&c.relay));r.fingerprint[0]='B';assert(!same(&r,&c.relay));cJSON_ReplaceItemInObject(j,"port",cJSON_CreateNumber(1883.5));assert(!parse(j,&c,1000));cJSON_ReplaceItemInObject(j,"port",cJSON_CreateNumber(1883));cJSON_ReplaceItemInObject(j,"rootTrusted",cJSON_CreateFalse());cJSON_AddBoolToObject(j,"adminConfirmed",true);assert(!parse(j,&c,1000));cJSON_AddBoolToObject(j,"mutualTrustConfirmed",true);assert(parse(j,&c,1000));assert(!usable(&c,1000));time_t until=0;assert(!auto_eligible(j,"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",1000,&until));cJSON_AddNumberToObject(j,"confirmedUntil",2000);cJSON*a=cJSON_AddArrayToObject(j,"acceptedClientRoots");cJSON_AddItemToArray(a,cJSON_CreateString("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));assert(auto_eligible(j,"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",1000,&until)&&until==2000);assert(!auto_eligible(j,"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",1000,&until));assert(!auto_eligible(j,"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",2000,&until));cJSON_Delete(j);puts("PASS discovery rejects name-only trust, stale/future announcements, unverified access and changed fingerprints");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+functions+tail);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),'-I'+str(cj),str(p),str(cj/'cJSON.c'),'-lm','-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
