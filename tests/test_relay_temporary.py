from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/relays.c').read_text()
functions='\n'.join(line for line in s.splitlines() if line.startswith(('int relays_current(','bool relays_select(','void relays_use_temporary(')))
pre='''#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct{int uid;}relay_t;
static relay_t rows[4]={{1},{2},{3},{4}},temporary;static bool temporary_active,fail;static int selected,count=4,saves;static uint32_t revision=1;
static bool persist(void){saves++;return !fail;}
'''
tail='''int main(void){relay_t r={9},out;uint32_t rev;relays_use_temporary(&r);assert(relays_current(&out,&rev)==-1&&out.uid==9&&rev==2&&saves==0&&count==4);fail=true;assert(!relays_select(2));assert(relays_current(&out,0)==-1&&out.uid==9&&selected==0);fail=false;assert(relays_select(2));assert(relays_current(&out,&rev)==2&&out.uid==3&&rev==3);assert(!relays_select(8));return 0;}'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(pre+functions+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
print('PASS full favorites allow temporary relay without overwrite, saved relay return and rollback')
