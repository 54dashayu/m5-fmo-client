/* Host libjpeg exercises the production JPEG container, CRC and capacity policy.
 * ESP-specific encoder/decoder is additionally probed on physical hardware. */
#include "sstv_store.h"
#include "sstv_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <jpeglib.h>
bool sstv_jpeg_encode(const uint8_t*p,uint8_t**out,size_t*n){
 struct jpeg_compress_struct c;struct jpeg_error_mgr err;c.err=jpeg_std_error(&err);jpeg_create_compress(&c);unsigned long size=0;*out=NULL;jpeg_mem_dest(&c,out,&size);c.image_width=320;c.image_height=240;c.input_components=3;c.in_color_space=JCS_RGB;jpeg_set_defaults(&c);for(int i=0;i<3;i++){c.comp_info[i].h_samp_factor=1;c.comp_info[i].v_samp_factor=1;}jpeg_set_quality(&c,90,TRUE);jpeg_start_compress(&c,TRUE);uint8_t row[960];while(c.next_scanline<240){for(int x=0;x<320;x++){unsigned at=(c.next_scanline*320+x)*2,v=p[at]|p[at+1]<<8;row[x*3]=(v>>11)*255/31;row[x*3+1]=((v>>5)&63)*255/63;row[x*3+2]=(v&31)*255/31;}JSAMPROW r=row;jpeg_write_scanlines(&c,&r,1);}jpeg_finish_compress(&c);jpeg_destroy_compress(&c);*n=size;return true;
}
bool sstv_jpeg_decode(const uint8_t*j,size_t n,uint8_t*p){
 struct jpeg_decompress_struct d;struct jpeg_error_mgr err;d.err=jpeg_std_error(&err);jpeg_create_decompress(&d);jpeg_mem_src(&d,j,n);jpeg_read_header(&d,TRUE);d.out_color_space=JCS_RGB;jpeg_start_decompress(&d);assert(d.output_width==320&&d.output_height==240);uint8_t row[960];while(d.output_scanline<240){unsigned y=d.output_scanline;JSAMPROW r=row;jpeg_read_scanlines(&d,&r,1);for(unsigned x=0;x<320;x++){unsigned v=(row[x*3]>>3)<<11|(row[x*3+1]>>2)<<5|(row[x*3+2]>>3),at=(y*320+x)*2;p[at]=v;p[at+1]=v>>8;}}jpeg_finish_decompress(&d);jpeg_destroy_decompress(&d);return true;
}
int main(void){
 char dir[]="/tmp/sstv-jpeg-store-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));uint8_t*p=malloc(SSTV_BYTES),*decoded=malloc(SSTV_BYTES);assert(p&&decoded);
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){unsigned v=(x*31/319)<<11|(y*63/239)<<5|((x+y)%32),at=(y*320+x)*2;p[at]=v;p[at+1]=v>>8;}
 uint32_t id;assert(sstv_store_save_relay(p,1794550720,&id,"Server / 1791"));sstv_entry_t entries[20];assert(sstv_store_list(entries)==1&&entries[0].compressed&&entries[0].width==320&&entries[0].height==240&&entries[0].file_bytes<153600);assert(!strcmp(entries[0].relay,"Server / 1791"));assert(sstv_store_load(id,decoded));
 char path[300];snprintf(path,sizeof(path),"%s/fmo-sstv-%08x.bin",dir,id);FILE*f=fopen(path,"r+b");assert(f);fseek(f,-1,SEEK_END);int b=fgetc(f);fseek(f,-1,SEEK_END);fputc(b^1,f);fclose(f);assert(!sstv_store_load(id,decoded));assert(sstv_store_delete(id));
 unsigned seed=42;for(unsigned i=0;i<SSTV_BYTES;i++){seed=seed*1664525+1013904223;p[i]=seed>>24;}
 unsigned count=0;while(sstv_store_save(p,1794550720,&id))count++;assert(count>0&&count<20);assert(sstv_store_list(entries)==(int)count);sstv_storage_t space;assert(sstv_store_stats(&space)&&space.used<space.safe_limit&&space.low);for(unsigned i=0;i<count;i++){assert(sstv_store_load(entries[i].id,decoded));assert(sstv_store_delete(entries[i].id));}assert(!rmdir(dir));free(p);free(decoded);puts("PASS real JPEG container: 320x240, metadata, CRC, reserved capacity, failed save preserves older images");
}
