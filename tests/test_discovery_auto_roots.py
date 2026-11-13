from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/discovery.c').read_text();same=s[s.index('static bool same('):s.index('static void status(')];fresh=s[s.index('static bool fresh('):s.index('static bool usable(')];fn=s[s.index('int discovery_accepted_roots('):]
pre=r'''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct{char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;}relay_t;
typedef struct {relay_t relay;time_t verified,attempted,seen;bool mutual;char accepted[10][44];int accepted_count;time_t accepted_until;} candidate_t;
static candidate_t rows[8];static int count=1,mutex=1;static time_t updated;
'''
tail=r'''int main(void){time_t now=time(NULL);updated=now;relay_t t={.uid=1845,.port=1883,.call="BI4TES",.host="example.net",.fingerprint="original"};rows[0].relay=t;rows[0].seen=now;rows[0].accepted_until=now+60;rows[0].accepted_count=1;strcpy(rows[0].accepted[0],"TES");char out[10][44];assert(discovery_accepted_roots(&t,out)==1&&!strcmp(out[0],"TES"));strcpy(t.fingerprint,"changed");assert(!discovery_accepted_roots(&t,out));t=rows[0].relay;t.port++;assert(!discovery_accepted_roots(&t,out));t=rows[0].relay;rows[0].accepted_until=now;assert(!discovery_accepted_roots(&t,out));rows[0].accepted_until=now+60;updated=now-600;assert(!discovery_accepted_roots(&t,out));updated=now;rows[0].seen=now+60;assert(!discovery_accepted_roots(&t,out));return 0;}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';p.write_text(pre+same+fresh+fn+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/t'],check=True);subprocess.run([d+'/t'],check=True)
print('PASS automatic root metadata binds exact server fingerprint, host, port, UID and fresh signed-catalog lifetime')
