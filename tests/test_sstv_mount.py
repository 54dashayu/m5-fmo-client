"""Exercise production mount/recovery logic without touching device flash."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/sstv_store.c').read_text()
code=s[s.index('const char *sstv_store_init('):s.index('static int list(')]
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define FMO_SSTV_STREAMING 1
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_NO_MEM 2
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
typedef struct {unsigned address,size;} esp_partition_t;
typedef struct {const char*base_path,*partition_label;int max_files;bool format_if_mount_failed;} esp_vfs_spiffs_conf_t;
typedef int DIR;
static int mutex,mount_result,read_result,formatted,blank,creates;static bool needs_initialization;
static char directory[256];static const char *error="uninitialized",*partition_label="spiffs";
static esp_partition_t part={0x400000,0x400000};
static int xSemaphoreCreateMutex(void){creates++;return 1;}
static bool take(void){return true;}static void give(void){}
static const esp_partition_t*esp_partition_find_first(int t,int sub,const char*l){(void)t;(void)sub;(void)l;return &part;}
static int esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t*c){assert(!c->format_if_mount_failed);return mount_result;}
static int esp_partition_read(const esp_partition_t*p,size_t o,void*b,size_t n){(void)p;(void)o;memset(b,blank?255:0,n);return read_result;}
static int esp_spiffs_format(const char*l){assert(!strcmp(l,"sstv"));formatted++;mount_result=0;return 0;}
static DIR *opendir(const char*p){(void)p;static DIR d;return &d;}static void closedir(DIR*d){(void)d;}static void unlink(const char*p){(void)p;}
'''
tail=r'''
int main(void){
 mount_result=ESP_ERR_NO_MEM;blank=0;assert(sstv_store_init(NULL));assert(!formatted&&!needs_initialization);
 mount_result=ESP_FAIL;read_result=3;assert(sstv_store_init(NULL));assert(!formatted&&!needs_initialization);
 read_result=0;assert(sstv_store_init(NULL));assert(!formatted&&needs_initialization);
 part.address=0x310000;assert(sstv_store_initialize_confirmed());assert(!formatted);
 part.address=0x400000;assert(!sstv_store_initialize_confirmed());assert(formatted==1&&!needs_initialization);
 assert(sstv_store_initialize_confirmed());assert(formatted==1);assert(creates==1);
 mount_result=ESP_FAIL;blank=1;assert(!sstv_store_init(NULL));assert(formatted==2);
 mount_result=0;blank=0;assert(!sstv_store_init(NULL));assert(formatted==2);
 puts("PASS mount: memory/read errors never erase; existing bytes require confirmation; exact album partition only; valid album preserved; mutex reused");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(pre+code+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
