from pathlib import Path
import subprocess,tempfile
src=(Path(__file__).resolve().parents[1]/'ui-preview-firmware/main/relays.c').read_text().split('void relays_connection(',1)[1]
prefix='''#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#define RELAY_MAX 4
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct {int uid,port;char host[64],fingerprint[44];} relay_t;
static relay_t rows[4]={{1791,1883,"example.org","first"},{99,1883,"example.net","second"}};
static int count=2,connection_result[4];static time_t connection_time[4];
'''
tail='''int main(void){relay_t t=rows[0];relays_connection(&t,true);assert(connection_result[0]==1&&connection_result[1]==0);strcpy(t.fingerprint,"different");relays_connection(&t,false);assert(connection_result[0]==1);t=rows[0];relays_connection(&t,false);assert(connection_result[0]==2&&connection_result[1]==0);relays_clear_evidence();assert(connection_result[0]==0&&connection_time[0]==0);return 0;}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(prefix+'void relays_connection('+src+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
print('PASS relay evidence matches exact target, ignores other identity, clears on certificate replacement')
