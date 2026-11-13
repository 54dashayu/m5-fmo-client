#include "sstv_store.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint8_t pixels[SSTV_BYTES],loaded[SSTV_BYTES];
int main(void){char dir[]="/tmp/sstv-part-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));memset(pixels,0x71,20*640);uint32_t id;assert(!sstv_store_save_partial(pixels,1,&id,"relay",0));assert(!sstv_store_save_partial(pixels,1,&id,"relay",240));assert(sstv_store_save_partial(pixels,123,&id,"relay",20));sstv_entry_t entries[20];assert(sstv_store_list(entries)==1&&entries[0].partial&&entries[0].received_lines==20);assert(!strcmp(entries[0].relay,"relay"));assert(sstv_store_load(id,loaded));assert(!memcmp(pixels,loaded,SSTV_BYTES));assert(sstv_store_delete(id));assert(sstv_store_save_relay(pixels,124,&id,"relay"));assert(sstv_store_list(entries)==1&&!entries[0].partial);assert(sstv_store_delete(id));rmdir(dir);return 0;}
