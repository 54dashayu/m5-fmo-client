"""Exercise actual local MQTT enqueue code with a busy transport lock."""
from pathlib import Path
import subprocess, tempfile
src=(Path(__file__).resolve().parents[1]/"ui-preview-firmware/components/mqtt/esp-mqtt/mqtt_client.c").read_text()
code=src[src.index("static int mqtt_enqueue_with_wait("):src.index("esp_err_t esp_mqtt_client_register_event(")]
prefix=r"""
#include <assert.h>
#include <stdbool.h>
#include <string.h>
typedef unsigned TickType_t;
#define portMAX_DELAY 0xffffffffu
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGE(...) ((void)0)
#define MQTT_API_UNLOCK(c) (++unlocks)
static int busy,unlocks,calls,bytes,result=7; static unsigned waited;
struct config {int outbox_limit;};
struct client {int api_lock; struct config *config; void *outbox;};
typedef struct client *esp_mqtt_client_handle_t;
static int xSemaphoreTakeRecursive(int lock,unsigned wait){(void)lock;waited=wait;return !busy;}
static int outbox_get_size(void *p){(void)p;return bytes;}
static int mqtt_client_enqueue_publish(esp_mqtt_client_handle_t c,const char*t,const char*d,int n,int q,int r,bool s){(void)c;(void)t;(void)d;(void)n;(void)q;(void)r;(void)s;calls++;return result;}
"""
main=r"""
int main(void){
 struct config cfg={2300};struct client c={0,&cfg,&cfg};
 busy=1;assert(esp_mqtt_client_enqueue_timeout(&c,"t","abc",3,0,0,true,20)==-3);assert(waited==20&&calls==0&&unlocks==0);
 assert(esp_mqtt_client_outbox_size_now(&c)==-1&&waited==0);
 busy=0;bytes=2299;assert(esp_mqtt_client_enqueue_timeout(&c,"t","abc",3,0,0,true,20)==-2);assert(unlocks==1&&calls==0);
 bytes=0;assert(esp_mqtt_client_enqueue_timeout(&c,"t","abc",3,0,0,true,20)==7);assert(unlocks==2&&calls==1);
 assert(esp_mqtt_client_enqueue(&c,"t","abc",3,0,0,true)==7&&waited==portMAX_DELAY);
 assert(esp_mqtt_client_enqueue_timeout(&c,"t","abc",3,0,0,true,51)==-1);
 assert(esp_mqtt_client_outbox_size_now(&c)==0&&waited==0);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/"test.c";p.write_text(prefix+code+main)
 subprocess.run(["cc","-Wall","-Wextra","-Werror",str(p),"-o",tmp+"/test"],check=True)
 subprocess.run([tmp+"/test"],check=True)
print("Bounded enqueue busy/full/success/query and legacy wrapper checks passed")
