#include "sstv_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint8_t pixels[SSTV_BYTES],loaded[SSTV_BYTES];
int main(void){
 char path[]="/tmp/fmo-gallery-XXXXXX";assert(mkdtemp(path));assert(!sstv_store_init(path));
 sstv_entry_t entries[SSTV_GALLERY_MAX];assert(sstv_store_list(entries)==0);
 for(int i=0;i<SSTV_BYTES;i++){pixels[i]=(i*37)^i;}
 uint32_t id=0;assert(sstv_store_save(pixels,1794500000,&id)&&id==1);assert(sstv_store_load(id,loaded));assert(!memcmp(pixels,loaded,sizeof(pixels)));
 assert(!sstv_store_init(path));assert(sstv_store_list(entries)==1&&entries[0].saved_at==1794500000);assert(sstv_store_load(1,loaded));
 for(unsigned i=2;i<=SSTV_GALLERY_MAX;i++){assert(sstv_store_save(pixels,1794500000+i,&id)&&id==i);}
 assert(!sstv_store_save(pixels,0,&id));assert(sstv_store_list(entries)==SSTV_GALLERY_MAX&&entries[0].id==SSTV_GALLERY_MAX);
 assert(sstv_store_delete(4));assert(!sstv_store_load(4,loaded));assert(!sstv_store_delete(0));assert(!sstv_store_load(0,loaded));
 assert(sstv_store_save(pixels,0,&id)&&id==SSTV_GALLERY_MAX+1);
 char file[300];snprintf(file,sizeof(file),"%s/fmo-sstv-00000001.bin",path);FILE*f=fopen(file,"r+b");assert(f);fseek(f,100,SEEK_SET);fputc(123,f);fclose(f);assert(!sstv_store_load(1,loaded));
 assert(sstv_store_delete(2));snprintf(file,sizeof(file),"%s/fmo-sstv-pending.tmp",path);f=fopen(file,"wb");assert(f);fputs("power loss partial record",f);fclose(f);
 assert(sstv_store_list(entries)==SSTV_GALLERY_MAX-1);assert(!sstv_store_init(path));assert(access(file,F_OK)!=0);assert(sstv_store_load(3,loaded));
 for(unsigned i=1;i<=SSTV_GALLERY_MAX+1;i++){sstv_store_delete(i);}assert(rmdir(path)==0);
 puts("PASS gallery: persisted pixels and timestamps, restart, capacity, deletion, CRC rejection, interrupted-save cleanup");
}
