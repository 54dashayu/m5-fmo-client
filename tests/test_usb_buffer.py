from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/device_web.c').read_text();code=s.split('static bool usb_append',1)[1].split('static void usb_task',1)[0]
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.c';p.write_text('''#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
static int fail;
static void *grow(void*p,size_t n){return fail?NULL:realloc(p,n);}
#define realloc grow
static bool usb_append'''+code+'''
int main(void){size_t cap=256,n=0;char*p=malloc(cap);for(int i=0;i<8192;i++)assert(usb_append(&p,&cap,&n,'A'));assert(n==8192&&cap==8193);p[n]=0;assert(!usb_append(&p,&cap,&n,'B'));for(size_t i=0;i<n;i++)assert(p[i]=='A');free(p);cap=256;n=255;p=malloc(cap);char*old=p;fail=1;assert(!usb_append(&p,&cap,&n,'C'));assert(p==old&&cap==256&&n==255);p[n]=0;free(p);}
''');subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
print('PASS USB 8192-byte request preserved, oversize rejected, allocation failure leaves original buffer valid')
