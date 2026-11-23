#include "sstv_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static bool source(void*ctx,uint8_t*p,size_t n){(void)ctx;memset(p,0x3c,n);return true;}
static bool fail_source(void*ctx,uint8_t*p,size_t n){(void)ctx;(void)p;(void)n;return false;}
int main(void){char dir[]="/tmp/e32-store-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));uint32_t id=0;sstv_entry_t entries[SSTV_GALLERY_MAX];
 assert(!sstv_store_import(fail_source,NULL,1,&id));assert(sstv_store_list(entries)==0);
 unsigned count=0;while(sstv_store_import(source,NULL,123,&id)){assert(++count<10);}
 assert(count>0&&sstv_store_list(entries)==(int)count);assert(strstr(sstv_store_save_error(),"空间"));
 assert(!sstv_store_init(dir));assert(sstv_store_list(entries)==(int)count);sstv_reader_t*r=sstv_store_reader_open(entries[0].id);assert(r);uint8_t row[640];assert(sstv_store_reader_row(r,239,row));for(unsigned i=0;i<sizeof(row);i++)assert(row[i]==0x3c);sstv_store_reader_close(r);
 for(unsigned i=0;i<count;i++){assert(sstv_store_delete(entries[i].id));}
 assert(!rmdir(dir));puts("PASS internal album: import failure, capacity rejection, reopen persistence, row reads");}
