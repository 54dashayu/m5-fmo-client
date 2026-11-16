"""Exercise the production MQTT event assembler with a smaller RX buffer."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/device_web.c').read_text()
start=s.index('else if(id==MQTT_EVENT_DATA)')
body=s[start:s.index('\nstatic char *mqtt_password',start)].strip()
# Remove the final brace of mqtt_event and retain its DATA branch verbatim.
body=body.removeprefix('else ')[:-1]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define VOICE_WIRE_PACKET_MAX 2048
#define MQTT_EVENT_DATA 1
struct event {int current_data_offset,total_data_len,topic_len,data_len;const char *topic,*data;};
static unsigned delivered,rejected;static size_t length;static uint8_t actual[2048];
static void voice_receive(const uint8_t*p,size_t n){delivered++;length=n;memcpy(actual,p,n);}
static void voice_reject_oversized(size_t n){assert(n>2048);rejected++;}
static void event(struct event *e){int id=MQTT_EVENT_DATA;static uint8_t packet[VOICE_WIRE_PACKET_MAX];static int received,total;static bool collecting;
'''
tail=r'''
}
int main(void){char input[2048];for(unsigned i=0;i<sizeof(input);i++)input[i]=(char)(i*37);
unsigned sizes[]={408,1096,2048};unsigned chunks[]={1,17,512,1012,1024};
for(unsigned z=0;z<3;z++)for(unsigned c=0;c<5;c++){delivered=0;unsigned n=sizes[z];for(unsigned at=0;at<n;){unsigned len=n-at;if(len>chunks[c])len=chunks[c];struct event e={at,n,at?0:7,len,at?NULL:"FMO/RAW",input+at};event(&e);at+=len;if(at<n)assert(!delivered);}assert(delivered==1&&length==n&&!memcmp(input,actual,n));}
delivered=0;struct event e={0,1096,7,512,"FMO/RAW",input};event(&e);e.current_data_offset=513;e.data_len=583;e.data=input+513;event(&e);assert(!delivered);
e=(struct event){0,2049,7,512,"FMO/RAW",input};event(&e);assert(rejected==1&&!delivered);
e=(struct event){0,408,7,408,"FMO/RAW",input};event(&e);assert(delivered==1&&length==408);
puts("PASS production MQTT assembler: 408/1096/2048 byte packets, tiny/1024 byte fragments, missing chunk, oversized rejection and recovery");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(pre+body+tail)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
