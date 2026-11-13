#include "sstv_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint8_t image[SSTV_BYTES],preview[SSTV_PREVIEW_BYTES];static size_t offset;
static bool writer(void*ctx,const uint8_t*p,size_t n){(void)ctx;assert(offset+n<=sizeof(image));assert(!memcmp(image+offset,p,n));offset+=n;return true;}
int main(void){char path[]="/tmp/sstv-stream-XXXXXX";assert(mkdtemp(path));assert(!sstv_store_init(path));
 for(unsigned i=0;i<sizeof(image);i++)image[i]=(i*17+i/31)&255;
 uint32_t id=0;sstv_entry_t entries[SSTV_GALLERY_MAX];assert(sstv_stream_prepare());assert(sstv_stream_begin("test / 1791"));assert(sstv_stream_rows(0,image));assert(!sstv_stream_finish(123));assert(!sstv_stream_save(&id));assert(sstv_store_list(entries)==0);sstv_stream_discard();
 assert(sstv_stream_prepare());assert(sstv_stream_begin("test / 1791"));
 for(unsigned row=0;row<240;row+=2)assert(sstv_stream_rows(row,image+row*640));
 assert(sstv_stream_finish(123));assert(sstv_store_list(entries)==0);assert(sstv_stream_save(&id));assert(id==1);assert(sstv_store_list(entries)==1);assert(entries[0].width==320&&entries[0].height==240&&entries[0].saved_at==123);assert(!strcmp(entries[0].relay,"test / 1791"));
 assert(sstv_store_export(id,writer,NULL));assert(offset==sizeof(image));assert(sstv_store_preview(id,preview));
 for(unsigned y=0;y<SSTV_PREVIEW_HEIGHT;y++)for(unsigned x=0;x<SSTV_PREVIEW_WIDTH;x++){unsigned at=((y*240/SSTV_PREVIEW_HEIGHT)*320+x*320/SSTV_PREVIEW_WIDTH)*2,v=image[at]|image[at+1]<<8,r=(v>>11)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;assert(preview[y*SSTV_PREVIEW_WIDTH+x]==((r&224)|((g>>3)&28)|(b>>6)));}
 char file[256];snprintf(file,sizeof(file),"%s/fmo-sstv-00000001.bin",path);FILE*f=fopen(file,"r+b");assert(f);fseek(f,170,SEEK_SET);fputc(0,f);fclose(f);offset=0;assert(!sstv_store_export(id,writer,NULL)&&offset==0);assert(sstv_store_delete(id));
assert(sstv_stream_prepare());assert(sstv_stream_begin("partial / 1791"));for(unsigned row=0;row<20;row+=2)assert(sstv_stream_rows(row,image+row*640));
assert(sstv_stream_finish_partial(456));assert(sstv_stream_save(&id));assert(sstv_store_list(entries)==1);assert(entries[0].partial&&entries[0].received_lines==20&&entries[0].saved_at==456);assert(!strcmp(entries[0].relay,"partial / 1791"));
memset(image+20*640,0,sizeof(image)-20*640);offset=0;assert(sstv_store_export(id,writer,NULL)&&offset==sizeof(image));assert(sstv_store_delete(id));rmdir(path);puts("PASS streaming save: interrupted/complete commit, exact full-resolution export, preview and corrupt-file rejection");}
