"""Compile the actual boot-time code loading block with durable NVS stubs."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/device_web.c').read_text()
block=s.split('size_t code_len=sizeof(code);',1)[1].split('\n',1)[0]
block='size_t code_len=sizeof(code);'+block
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c'
 p.write_text('''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define ESP_OK 0
static char stored[17],code[17];static int nv,fail,commits;
static int nvs_get_str(int h,const char*k,char*out,size_t*n){(void)h;(void)k;if(!stored[0])return 1;*n=strlen(stored)+1;memcpy(out,stored,*n);return 0;}
static int nvs_set_str(int h,const char*k,const char*v){(void)h;(void)k;if(fail)return fail;strcpy(stored,v);return 0;}
static int nvs_commit(int h){(void)h;commits++;return fail;}
static void crypto_wipe(void*p,size_t n){memset(p,0,n);}
static int boot(uint8_t entropy){uint8_t random[8];memset(random,entropy,8);int e=0;
'''+block+'''
return 0;}
int main(void){assert(!boot(3));assert(!strcmp(code,"33333333"));assert(commits==1);memset(code,0,sizeof(code));assert(!boot(8));assert(!strcmp(code,"33333333"));assert(commits==1);stored[0]=0;assert(!boot(9));assert(!strcmp(code,"99999999"));strcpy(stored,"notvalid");assert(!boot(4));assert(!strcmp(code,"44444444"));stored[0]=0;fail=7;assert(boot(5)==7);}
''')
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
print('PASS fixed management code survives reboot, rotates after removal, rejects malformed storage and fails closed on write failure')

# Browser-side reveal must require an explicit click; use only a mock code.
import ast,pathlib,subprocess,tempfile
p=root/'tools/usb_admin.py';tree=ast.parse(p.read_text())
panel=next(n.value.value for n in ast.walk(tree) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='panel' for t in n.targets))
js=panel.split('<script>')[1].split('</script>')[0]
pre="const assert=require('assert');let requests=[],result={textContent:''},button={};const document={getElementById:id=>id==='usbRevealCode'?button:result};let fail=false;async function fetch(path,args){requests.push([path,args]);return {ok:!fail,json:async()=>path==='/api/transport'?{token:'mock-session'}:fail?{error:'unsupported'}:{code:'12345678'}};}\n"
post="\n(async()=>{assert.equal(requests.length,0);await button.onclick();assert.equal(result.textContent,'12345678');assert.equal(requests[1][1].headers['X-Device-Token'],'mock-session');fail=true;await button.onclick();assert.equal(result.textContent,'unsupported');console.log('PASS explicit USB reveal, session header and error handling (mock code only)')})().catch(e=>{console.error(e);process.exit(1)});"
with tempfile.NamedTemporaryFile(suffix='.js',mode='w') as f:
 f.write(pre+js+post);f.flush();subprocess.run(['node',f.name],check=True)
