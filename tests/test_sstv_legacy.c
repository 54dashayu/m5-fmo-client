/* Verify old persisted photos are readable, never rewritten, with guarded 565 output. */
#include "sstv_store.c"
#include <assert.h>
static uint8_t old[19200],out[SSTV_BYTES+16];
int main(void){char dir[]="/tmp/sstv-legacy-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));for(unsigned i=0;i<19200;i++)old[i]=(i*71+13)&255;
char path[300];filename(path,1);FILE*f=fopen(path,"wb");assert(f);uint8_t h[24]={0};memcpy(h,"SSTV332\1",8);put(h+8,1);put(h+12,123);put(h+16,19200);put(h+20,crc(old,sizeof(old)));assert(fwrite(h,1,24,f)==24&&fwrite(old,1,sizeof(old),f)==sizeof(old));fclose(f);
memset(out,0xa5,sizeof(out));assert(sstv_store_load(1,out));for(int y=0;y<240;y++)for(int x=0;x<320;x++){unsigned v=old[(y/2)*160+x/2],c=(((v>>5)*255/7>>3)<<11)|(((((v>>2)&7)*255/7)>>2)<<5)|((v&3)*255/3>>3),at=(y*320+x)*2;assert(out[at]==(c&255)&&out[at+1]==c>>8);}for(unsigned i=SSTV_BYTES;i<sizeof(out);i++)assert(out[i]==0xa5);
sstv_entry_t entries[SSTV_GALLERY_MAX];assert(sstv_store_list(entries)==1&&entries[0].width==160&&!entries[0].relay[0]);uint32_t id;assert(sstv_store_save_relay(out,456,&id,"Test relay / 1791"));assert(sstv_store_list(entries)==2&&entries[0].width==320&&!strcmp(entries[0].relay,"Test relay / 1791"));assert(sstv_store_load(id,out));
f=fopen(path,"rb");assert(f);uint8_t same[24];assert(fread(same,1,24,f)==24&&!memcmp(same,h,24));fseek(f,0,SEEK_END);assert(ftell(f)==19224);fclose(f);sstv_store_delete(1);sstv_store_delete(2);assert(rmdir(dir)==0);puts("PASS legacy RGB332 exact expansion, unchanged old file, bounds and new relay metadata");}
