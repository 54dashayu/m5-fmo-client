"""Exercise the actual ESP-MQTT receive-decision prefix with delayed TCP chunks."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
PATCHED=ROOT/'ui-preview-firmware/components/mqtt/esp-mqtt/mqtt_client.c'
def harness(path):
    source=path.read_text()
    start=source.index('static inline bool has_timed_out(')
    timer=source[start:source.index('\nstatic esp_err_t process_keepalive',start)]
    start=source.index('static esp_err_t mqtt_process_receive(')
    receive=source[start:source.index('    int read_len = client->mqtt_state.message_length;',start)]+'    return ESP_OK;\n}\n'
    return '''#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
struct cfg { int network_timeout_ms; };
struct client { struct { size_t in_buffer_read_len; } mqtt_state; struct cfg *config; int64_t receive_progress_tick; };
typedef struct client *esp_mqtt_client_handle_t;
static uint64_t tick; static int next_result; static size_t next_length;
static uint64_t platform_tick_get_ms(void){return tick;}
static int mqtt_message_receive(esp_mqtt_client_handle_t c,int timeout){(void)timeout;c->mqtt_state.in_buffer_read_len=next_length;return next_result;}
'''+timer+receive+'''
static int step(struct client *c,uint64_t now,int result,size_t length){tick=now;next_result=result;next_length=length;return mqtt_process_receive(c);}
int main(void){
 struct cfg cfg={10000};struct client c={.config=&cfg};
 assert(step(&c,100,0,1024)==ESP_OK); // first part of a 1096-byte voice packet
 assert(step(&c,120,-1,1024)==ESP_OK); // one empty 20 ms poll must not disconnect
 assert(step(&c,1000,-1,1024)==ESP_OK);
 assert(step(&c,4100,1,1108)==ESP_OK); // delayed final TCP segment completes
 c.mqtt_state.in_buffer_read_len=0;
 assert(step(&c,5000,-1,1)==ESP_OK); // fragmented MQTT fixed header
 assert(step(&c,5100,-1,2)==ESP_OK); // progress resets the idle deadline
 assert(step(&c,15099,-1,2)==ESP_OK);
 assert(step(&c,15100,-1,2)==ESP_FAIL); // truly stalled partial packet expires
 c.mqtt_state.in_buffer_read_len=0;
 assert(step(&c,30000,0,0)==ESP_OK); // no packet is normal idle
 assert(step(&c,30001,-2,0)==ESP_FAIL); // real transport EOF still fails
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.c';exe=Path(tmp)/'test'
    src.write_text(harness(PATCHED))
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wno-unused-variable',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS actual MQTT receive logic: delayed TCP fragments, progress deadline, true timeout, idle and EOF')
