#include "sstv_store.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static unsigned received;
static bool source(void *ctx,uint8_t *p,size_t n){(void)ctx;for(size_t i=0;i<n;i++)p[i]=(received+i)%251;received+=n;return true;}
static bool failure(void *ctx,uint8_t *p,size_t n){(void)ctx;(void)p;(void)n;return false;}
int main(void){
 char dir[]="/tmp/fmo-sstv-tx-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));uint32_t id;
 assert(sstv_store_import(source,NULL,123456,&id)&&id==1&&received==153600);
 sstv_reader_t *r=sstv_store_reader_open(id);assert(r);uint8_t row[640];for(unsigned y=0;y<240;y++){assert(sstv_store_reader_row(r,y,row));for(unsigned x=0;x<640;x++)assert(row[x]==(y*640+x)%251);}unsigned order[]={0,239,17,18,18,0};for(unsigned k=0;k<sizeof(order)/sizeof(order[0]);k++){unsigned y=order[k];assert(sstv_store_reader_row(r,y,row));for(unsigned x=0;x<640;x++)assert(row[x]==(y*640+x)%251);}assert(!sstv_store_reader_row(r,240,row));sstv_store_reader_close(r);
 uint8_t thumb[80*60];assert(sstv_store_thumbnail(1,thumb,80,60));for(unsigned y=0;y<60;y++)for(unsigned x=0;x<80;x++){unsigned at=(y*4*320+x*4)*2,v=at%251|((at+1)%251)<<8,r=(v>>11)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;assert(thumb[y*80+x]==((r&224)|((g>>3)&28)|(b>>6)));}
 char cache[300];snprintf(cache,sizeof(cache),"%s/fmo-thumb-00000001.bin",dir);FILE*cf=fopen(cache,"rb");assert(cf);fseek(cf,0,SEEK_END);assert(ftell(cf)==4828);fclose(cf);
 uint8_t side[40*30];assert(sstv_store_thumbnail(1,side,40,30));for(unsigned y=0;y<30;y++)for(unsigned x=0;x<40;x++)assert(side[y*40+x]==thumb[y*2*80+x*2]);
 cf=fopen(cache,"r+b");assert(cf);fseek(cf,25,SEEK_SET);fputc(42,cf);fclose(cf);assert(sstv_store_thumbnail(1,side,40,30));for(unsigned y=0;y<30;y++)for(unsigned x=0;x<40;x++)assert(side[y*40+x]==thumb[y*2*80+x*2]);
 /* A cache validates itself and source metadata, not a fresh scan of the source payload. */
 assert(!unlink(cache));
 assert(!sstv_store_import(failure,NULL,123457,&id));sstv_entry_t entries[SSTV_GALLERY_MAX];assert(sstv_store_list(entries)==1);
 char path[300];snprintf(path,sizeof(path),"%s/fmo-sstv-00000001.bin",dir);FILE*f=fopen(path,"r+b");assert(f);assert(!fseek(f,200,SEEK_SET));fputc(42,f);fclose(f);assert(!sstv_store_reader_open(1));assert(!sstv_store_thumbnail(1,thumb,80,60));for(unsigned i=0;i<sizeof(thumb);i++)assert(!thumb[i]);assert(sstv_store_delete(1));assert(!sstv_store_reader_open(1));
 /* Larger pre-existing albums page without allocating a frame or full index. */
 for(unsigned i=1;i<=45;i++){received=0;assert(sstv_store_import(source,NULL,i,&id));char from[300],to[300];snprintf(from,sizeof(from),"%s/fmo-sstv-%08x.bin",dir,id);snprintf(to,sizeof(to),"%s/saved-%u",dir,i);assert(!rename(from,to));}
 for(unsigned i=1;i<=45;i++){char from[300],to[300];snprintf(from,sizeof(from),"%s/saved-%u",dir,i);snprintf(to,sizeof(to),"%s/fmo-sstv-%08x.bin",dir,i);assert(!rename(from,to));f=fopen(to,"r+b");assert(f);uint8_t value[4]={i,0,0,0};fseek(f,8,SEEK_SET);fwrite(value,1,4,f);fclose(f);}
 assert(sstv_store_neighbor(45,true)==1&&sstv_store_neighbor(1,false)==45);
 assert(sstv_store_neighbor(26,false)==25&&sstv_store_neighbor(26,true)==27);
 assert(sstv_store_page(entries,0)==20&&entries[0].id==45&&entries[19].id==26);
 assert(sstv_store_page(entries,26)==20&&entries[0].id==25&&entries[19].id==6);
 assert(sstv_store_page(entries,6)==5&&entries[0].id==5&&entries[4].id==1);
 assert(sstv_store_newer(entries,5)==20&&entries[0].id==25&&entries[19].id==6);
 assert(sstv_store_newer(entries,0)==20&&entries[0].id==20&&entries[19].id==1);
 for(unsigned i=1;i<=45;i++){assert(sstv_store_delete(i));}
 assert(!rmdir(dir));puts("TX store: import, row reads, abort, corruption, missing image and 45-image paging passed");
}
