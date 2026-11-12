"""Identity replacement invalidates access evidence but retains relay addresses."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/discovery.c').read_text();start=s.index('void discovery_identity_changed(void)');end=s.index('\nint discovery_list',start);fn=s[start:end]
code='''#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#define portMAX_DELAY 0
static atomic_bool owner_valid=true,refresh=false;
static int mutex=1,count=2,checking_uid=1791,revision=7;
static char owner[65]="old identity",message[128];
static struct {int uid,verified,attempted;} rows[2]={{1791,10,10},{730,20,20}};
static void xSemaphoreTake(int a,int b){(void)a;(void)b;}
static void xSemaphoreGive(int a){(void)a;}
FUNCTION
int main(void){discovery_identity_changed();assert(!owner_valid&&refresh&&!owner[0]&&!checking_uid&&revision==8);assert(rows[0].uid==1791&&rows[1].uid==730);for(int i=0;i<2;i++)assert(!rows[i].verified&&!rows[i].attempted);mutex=0;discovery_identity_changed();puts("PASS identity change invalidates old access evidence and preserves relay addresses");}
'''.replace('FUNCTION',fn)
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.c').write_text(code);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
