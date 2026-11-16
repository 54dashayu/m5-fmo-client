#include "sstv_store.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#if defined(FMO_FULL_SSTV) && !defined(FMO_SSTV_STREAMING) && (!defined(SSTV_HOST_TEST) || defined(SSTV_CODEC_HOST_TEST))
#include "sstv_codec.h"
#define SSTV_JPEG_STORAGE 1
#endif
#ifndef SSTV_HOST_TEST
#include "esp_spiffs.h"
#include "sstv_sd.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t mutex;
static const char *partition_label="spiffs";
static bool take(void){return mutex&&xSemaphoreTake(mutex,pdMS_TO_TICKS(100))==pdTRUE;}
static void give(void){xSemaphoreGive(mutex);}
#else
static bool take(void){return true;}static void give(void){}
#endif
static bool on_sd;
#ifndef SSTV_HOST_TEST
static bool force_flash;
#endif
#include <stdatomic.h>
static atomic_uint storage_revision=1;
unsigned sstv_store_revision(void){return atomic_load(&storage_revision);}
static unsigned open_readers;
static unsigned album_count;
bool sstv_store_on_sd(void){return on_sd;}
static bool needs_initialization;
bool sstv_store_needs_initialization(void){return needs_initialization;}
// All users hold the store mutex; keep the album index off task stacks.
static sstv_entry_t entry_scratch[SSTV_GALLERY_MAX];
static char directory[256];static const char *error="相册尚未初始化";
static const char *save_error="保存失败：相册已满或存储忙";
const char *sstv_store_save_error(void){return save_error;}
static uint32_t crc_update(uint32_t c,const uint8_t*p,size_t n){while(n--){c^=*p++;for(int b=0;b<8;b++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return c;}
static uint32_t crc(const uint8_t*p,size_t n){return ~crc_update(~0u,p,n);}
static void put(uint8_t*p,uint32_t v){for(int k=0;k<4;k++)p[k]=v>>(8*k);}
static uint32_t get(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void filename(char out[300],uint32_t id){snprintf(out,300,"%s/fmo-sstv-%08lx.bin",directory,(unsigned long)id);}
static bool header(FILE*f,uint8_t h[24]){if(fread(h,1,24,f)!=24)return false;
 unsigned version=h[7]&127;(void)version;
 if(!memcmp(h,"SSTV332",7)&&h[7]==1&&get(h+16)==19200)return true;
#ifdef FMO_FULL_SSTV
 if(!memcmp(h,"SSTVJPG",7)&&version==3&&get(h+16)>0&&get(h+16)<=160*1024)return true;
 if((!memcmp(h,"SSTV565",7)&&(version==1||version==2))&&get(h+16)==153600)return true;
#endif
 return false;}

const char *sstv_store_error(void){return error;}
const char *sstv_store_init(const char *path){
#ifndef SSTV_HOST_TEST
    (void)path;needs_initialization=false;if(!mutex)mutex=xSemaphoreCreateMutex();if(!mutex)return error="相册内存不足";
    path=force_flash?NULL:sstv_sd_mount();on_sd=path!=NULL;
    if(!on_sd){
    const esp_partition_t*p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
#ifdef FMO_SSTV_STREAMING
    const esp_partition_t*extra=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"sstv");
    if(extra){p=extra;partition_label="sstv";}
#endif
    if(!p)return error="缺少相册分区，请按升级说明安装";
    esp_vfs_spiffs_conf_t conf={.base_path="/sstv",.partition_label=partition_label,.max_files=3,.format_if_mount_failed=false};
    esp_err_t mounted=esp_spiffs_mounted(partition_label)?ESP_OK:esp_vfs_spiffs_register(&conf);
    if(mounted!=ESP_OK){
        ESP_LOGE("sstv_store","mount %s offset=0x%lx size=0x%lx error=%s",partition_label,(unsigned long)p->address,(unsigned long)p->size,esp_err_to_name(mounted));
        if(mounted==ESP_ERR_NO_MEM)return error="相册内存不足，请重启重试";
        if(mounted!=ESP_FAIL)return error="相册挂载失败，请重启检查";
        /* Never auto-format existing or corrupt data. Only a wholly erased partition. */
        uint8_t block[512];bool blank=true;
        for(size_t off=0;off<p->size&&blank;off+=sizeof(block)){
            if(esp_partition_read(p,off,block,sizeof(block))!=ESP_OK)return error="相册读取失败，请检查存储";
            for(unsigned k=0;k<sizeof(block);k++)if(block[k]!=255){blank=false;break;}
        }
        if(!blank){
#ifdef FMO_SSTV_STREAMING
            needs_initialization=!strcmp(partition_label,"sstv")&&p->address==0x400000&&p->size==0x400000;
#endif
            return error="相册未就绪，请到网页相册处理";
        }
        if(esp_spiffs_format(partition_label)!=ESP_OK||esp_vfs_spiffs_register(&conf)!=ESP_OK)return error="相册分区初始化失败";
    }
    path="/sstv";
    }
#endif
    if(!path||strlen(path)>=sizeof(directory))return error="相册路径无效";
    snprintf(directory,sizeof(directory),"%s",path);DIR*d=opendir(directory);if(!d)return error="相册存储不可用";closedir(d);
    char tmp[300];snprintf(tmp,sizeof(tmp),"%s/fmo-sstv-pending.tmp",directory);unlink(tmp);error=NULL;return NULL;
}

const char *sstv_store_initialize_confirmed(void){
#if !defined(SSTV_HOST_TEST) && defined(FMO_SSTV_STREAMING)
 if(!needs_initialization||!error)return "相册不需要初始化";
 const esp_partition_t*p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"sstv");
 if(!p||p->address!=0x400000||p->size!=0x400000||strcmp(partition_label,"sstv"))return "相册分区不匹配，未修改";
 if(!take())return "相册忙，请稍后重试";
 esp_err_t e=esp_spiffs_format("sstv");give();
 if(e!=ESP_OK)return "相册初始化失败，请重启检查";
 return sstv_store_init(NULL);
#else
 return "本设备不支持此初始化操作";
#endif
}

static int list_page(sstv_entry_t*out,uint32_t before,bool newer){
    album_count=0;
    DIR*d=opendir(directory);if(!d)return -1;int count=0;struct dirent*e;
    while((e=readdir(d))){unsigned id;int end=0;if(sscanf(e->d_name,"fmo-sstv-%8x.bin%n",&id,&end)!=1||end!=21||e->d_name[end]||!id)continue;
        char path[300];filename(path,id);FILE*f=fopen(path,"rb");if(!f)continue;uint8_t h[24]={0};bool valid=header(f,h)&&get(h+8)==id;char relay[128]={0};if(valid&&h[7]>=2)valid=fread(relay,1,128,f)==128;relay[127]=0;unsigned received=(h[7]&128)?get((const uint8_t*)relay+124):240;if(h[7]&128){relay[120]=0;valid=valid&&received>0&&received<240;}fclose(f);if(!valid)continue;
        album_count++;if(before&&(newer?id<=before:id>=before))continue;
        if(count==SSTV_GALLERY_MAX){int oldest=0;for(int k=1;k<count;k++)if(newer?out[k].id>out[oldest].id:out[k].id<out[oldest].id)oldest=k;if(newer?id>=out[oldest].id:id<=out[oldest].id)continue;out[oldest]=out[--count];}
        out[count]=(sstv_entry_t){.id=id,.saved_at=get(h+12),.width=!memcmp(h,"SSTV332",7)?160:320,.height=!memcmp(h,"SSTV332",7)?120:240,.file_bytes=24+(h[7]>=2?128:0)+get(h+16),.compressed=(h[7]&127)==3,.partial=(h[7]&128)!=0,.received_lines=received};memcpy(out[count++].relay,relay,128);
    }
    closedir(d);for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(out[j].id>out[i].id){sstv_entry_t x=out[i];out[i]=out[j];out[j]=x;}return count;
}
static int list(sstv_entry_t*out){return list_page(out,0,false);}
int sstv_store_page(sstv_entry_t*out,uint32_t before){if(error||!out||!take())return -1;int n=list_page(out,before,false);give();return n;}
int sstv_store_newer(sstv_entry_t*out,uint32_t after){if(error||!out||!take())return -1;int n=list_page(out,after,true);give();return n;}
int sstv_store_list(sstv_entry_t*out){if(error||!take())return -1;int n=list(out);give();return n;}
uint32_t sstv_store_neighbor(uint32_t id,bool newer){
 if(error||!id||!take())return 0;
 int n=list_page(entry_scratch,id,newer);
 if(n==0)n=list_page(entry_scratch,0,newer);
 uint32_t result=n>0?entry_scratch[newer?n-1:0].id:0;
 give();return result;
}
static bool storage_stats(sstv_storage_t*out,const sstv_entry_t*entries,int n){
 memset(out,0,sizeof(*out));if(n<0)return false;
#ifndef SSTV_HOST_TEST
 if(on_sd){if(!sstv_sd_space(&out->total,&out->used))return false;out->safe_limit=out->total>1024*1024?out->total-1024*1024:0;}else{
 size_t total=0,used=0;if(esp_spiffs_info(partition_label,&total,&used)!=ESP_OK)return false;
 const esp_partition_t*part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,partition_label);if(!part)return false;
 out->total=part->size;out->used=used;out->safe_limit=part->size*7/10;if(out->safe_limit>total)out->safe_limit=total;}
#else
 out->total=1024*1024;out->safe_limit=out->total*7/10;for(int i=0;i<n;i++)out->used+=entries[i].file_bytes;
#endif
#ifndef FMO_SSTV_STREAMING
 size_t bytes=0;unsigned compressed=0;for(int i=0;i<n;i++)if(entries[i].compressed){bytes+=entries[i].file_bytes;compressed++;}
#endif
#ifdef FMO_SSTV_STREAMING
 size_t unit=SSTV_BYTES*5/4+4096;
#else
 size_t unit=compressed?(bytes/compressed)*5/4+4096:64*1024;
#endif
 if(unit<32*1024)unit=32*1024;
 out->count=album_count;out->remaining=out->used<out->safe_limit?(out->safe_limit-out->used)/unit:0;
 if(!on_sd){unsigned slots=out->count<SSTV_GALLERY_MAX?SSTV_GALLERY_MAX-out->count:0;if(out->remaining>slots)out->remaining=slots;}
 out->low=out->remaining<=2;return true;
}
bool sstv_store_stats(sstv_storage_t*out){if(error||!out||!take())return false;sstv_entry_t *entries=entry_scratch;int n=list(entries);bool ok=storage_stats(out,entries,n);give();return ok;}
static bool save_frame(const uint8_t*p,uint32_t at,uint32_t*id,const char*relay,unsigned partial_lines){
    (void)relay;(void)partial_lines;
    if(error||!p||!id||!take())return false;
    sstv_entry_t *entries=entry_scratch;int n=list(entries);bool ok=false;uint8_t*encoded=NULL;const uint8_t*payload=p;size_t payload_size=SSTV_BYTES;save_error="保存失败：相册已满或存储忙";
    if(n<0||(!on_sd&&album_count>=SSTV_GALLERY_MAX))goto done;
#ifdef SSTV_JPEG_STORAGE
    if(!sstv_jpeg_encode(p,&encoded,&payload_size)){save_error="图片压缩失败，请稍后保存";goto done;}payload=encoded;
    sstv_storage_t space;if(!storage_stats(&space,entries,n)||space.used+payload_size*5/4+4096>space.safe_limit){save_error="相册空间不足，请先导出并删除旧图";goto done;}
#endif
    uint32_t next=n?entries[0].id+1:1;if(!next)goto done;
    char path[300],tmp[300];filename(path,next);snprintf(tmp,sizeof(tmp),"%s/fmo-sstv-pending.tmp",directory);
    FILE*f=fopen(tmp,"wb");if(!f)goto done;
    uint8_t h[24]={0};
#ifdef FMO_FULL_SSTV
#ifdef SSTV_JPEG_STORAGE
    memcpy(h,"SSTVJPG\3",8);
#else
    memcpy(h,"SSTV565\2",8);
#endif
#else
    memcpy(h,"SSTV332\1",8);
#endif
put(h+8,next);put(h+12,at);put(h+16,payload_size);put(h+20,crc(payload,payload_size));
#ifdef FMO_FULL_SSTV
    char context[128]={0};if(relay)snprintf(context,partial_lines?121:sizeof(context),"%s",relay);
    if(partial_lines){h[7]|=128;put((uint8_t*)context+124,partial_lines);}
    put(h+20,~crc_update(crc_update(~0u,(const uint8_t*)context,128),payload,payload_size));
#endif
    ok=fwrite(h,1,24,f)==24;
#ifdef FMO_FULL_SSTV
    if(fwrite(context,1,128,f)!=128)ok=false;
#endif
    if(fwrite(payload,1,payload_size,f)!=payload_size||fflush(f))ok=false;
    if(fsync(fileno(f)))ok=false;
    if(fclose(f))ok=false;
    if(ok)ok=rename(tmp,path)==0;
    if(ok)*id=next;else unlink(tmp);
done:free(encoded);give();return ok;
}
bool sstv_store_save_relay(const uint8_t*p,uint32_t at,uint32_t*id,const char*relay){return save_frame(p,at,id,relay,0);}
bool sstv_store_save_partial(const uint8_t*p,uint32_t at,uint32_t*id,const char*relay,unsigned lines){return lines>0&&lines<240&&save_frame(p,at,id,relay,lines);}
bool sstv_store_load(uint32_t id,uint8_t*p){
    if(error||!id||!p||!take())return false;
    char path[300];filename(path,id);FILE*f=fopen(path,"rb");bool ok=false;
    if(f){uint8_t h[24];ok=header(f,h)&&get(h+8)==id;
        unsigned bytes=ok?get(h+16):0;
        uint32_t sum=~0u;
        if(ok&&h[7]>=2){uint8_t context[128];ok=fread(context,1,128,f)==128;if(ok)sum=crc_update(sum,context,128);}
        uint8_t*packed=ok&&(h[7]&127)==3?malloc(bytes):NULL;uint8_t*target=ok&&(h[7]&127)==3?packed:p;
        if(ok)ok=target&&fread(target,1,bytes,f)==bytes&&fgetc(f)==EOF&&~crc_update(sum,target,bytes)==get(h+20);
        if(ok&&(h[7]&127)==3){
#ifdef SSTV_JPEG_STORAGE
            ok=sstv_jpeg_decode(packed,bytes,p);
#else
            ok=false;
#endif
        }
        free(packed);
#ifdef FMO_FULL_SSTV
        if(ok&&!memcmp(h,"SSTV332",7)){for(int y=239;y>=0;y--)for(int x=319;x>=0;x--){unsigned v=p[(y/2)*160+x/2];unsigned c=(((v>>5)*255/7>>3)<<11)|(((((v>>2)&7)*255/7)>>2)<<5)|((v&3)*255/3>>3);unsigned at=(y*320+x)*2;p[at]=c;p[at+1]=c>>8;}}
#endif
        fclose(f);}
    give();return ok;
}
bool sstv_store_delete(uint32_t id){if(error||!id||!take())return false;char path[300];filename(path,id);bool ok=unlink(path)==0;if(ok){snprintf(path,sizeof(path),"%s/fmo-thumb-%08lx.bin",directory,(unsigned long)id);unlink(path);}give();return ok;}

bool sstv_store_save(const uint8_t*p,uint32_t at,uint32_t*id){return sstv_store_save_relay(p,at,id,"");}

#include "sstv_stream_store.inc"

struct sstv_reader { FILE *file; uint8_t *pixels; long offset; bool small; };
void sstv_store_reader_close(sstv_reader_t *r){if(!r)return;if(r->file)fclose(r->file);free(r->pixels);free(r);
#ifndef SSTV_HOST_TEST
 xSemaphoreTake(mutex,portMAX_DELAY);
#endif
 if(open_readers)open_readers--;
 give();}
sstv_reader_t *sstv_store_reader_open(uint32_t id){
 if(error||!id||!take())return NULL;
 sstv_reader_t *r=calloc(1,sizeof(*r));bool ok=false;
 if(r){char path[300];filename(path,id);r->file=fopen(path,"rb");
  if(r->file&&on_sd)setvbuf(r->file,NULL,_IONBF,0); /* Avoid 4 KiB read-ahead stalls on slow shared SPI. */
  if(r->file){uint8_t h[24],buf[1280];ok=header(r->file,h)&&get(h+8)==id;uint32_t sum=~0u;
   if(ok&&h[7]>=2){ok=fread(buf,1,128,r->file)==128;if(ok)sum=crc_update(sum,buf,128);}
   r->offset=ftell(r->file);size_t bytes=ok?get(h+16):0;r->small=bytes==19200;
   for(size_t left=bytes;ok&&left;){size_t n=left>sizeof(buf)?sizeof(buf):left;ok=fread(buf,1,n,r->file)==n;if(ok)sum=crc_update(sum,buf,n);left-=n;}
   ok=ok&&fgetc(r->file)==EOF&&~sum==get(h+20);
   if(ok&&(h[7]&127)==3){
#ifdef SSTV_JPEG_STORAGE
    uint8_t *jpg=malloc(bytes);r->pixels=malloc(153600);ok=jpg&&r->pixels&&fseek(r->file,r->offset,SEEK_SET)==0&&fread(jpg,1,bytes,r->file)==bytes&&sstv_jpeg_decode(jpg,bytes,r->pixels);free(jpg);
#else
    ok=false;
#endif
   }
  }
 }
 if(ok)open_readers++;
 give();if(!ok){if(r){if(r->file)fclose(r->file);free(r->pixels);free(r);}r=NULL;}return r;
}
bool sstv_store_reader_row(void *ctx,unsigned row,uint8_t *pixels){
 sstv_reader_t *r=ctx;if(!r||row>=240||!pixels)return false;
 if(r->pixels){memcpy(pixels,r->pixels+row*640,640);return true;}
 if(r->small){uint8_t line[160];if(fseek(r->file,r->offset+(row/2)*160,SEEK_SET)||fread(line,1,160,r->file)!=160)return false;
  for(unsigned x=0;x<320;x++){unsigned v=line[x/2],c=(((v>>5)*31/7)<<11)|((((v>>2)&7)*63/7)<<5)|((v&3)*31/3);pixels[x*2]=c;pixels[x*2+1]=c>>8;}return true;}
 return fseek(r->file,r->offset+row*640,SEEK_SET)==0&&fread(pixels,1,640,r->file)==640;
}

bool sstv_store_import(sstv_import_read_t read,void *ctx,uint32_t at,uint32_t *id){
 if(error||!read||!id||!take())return false;
 int n=list(entry_scratch);sstv_storage_t space;bool ok=storage_stats(&space,entry_scratch,n)&&(on_sd||album_count<SSTV_GALLERY_MAX)&&space.used+153600*5/4+4096<=space.safe_limit;
 uint32_t next=n>0?entry_scratch[0].id+1:1;ok=ok&&next;
 char tmp[300],path[300];snprintf(tmp,sizeof(tmp),"%s/fmo-upload.tmp",directory);filename(path,next);
 FILE *f=ok?fopen(tmp,"wb"):NULL;ok=ok&&f;
 if(f){uint8_t h[24]={0},context[128]={0},buf[1280];memcpy(h,"SSTV565\2",8);put(h+8,next);put(h+12,at);put(h+16,153600);
  uint32_t sum=crc_update(~0u,context,128);ok=fwrite(h,1,24,f)==24&&fwrite(context,1,128,f)==128;
  for(unsigned y=0;ok&&y<120;y++){ok=read(ctx,buf,sizeof(buf))&&fwrite(buf,1,sizeof(buf),f)==sizeof(buf);if(ok)sum=crc_update(sum,buf,sizeof(buf));}
  put(h+20,~sum);if(ok)ok=fseek(f,0,SEEK_SET)==0&&fwrite(h,1,24,f)==24&&fflush(f)==0&&fsync(fileno(f))==0;
  if(fclose(f))ok=false;
  if(ok)ok=rename(tmp,path)==0;
 }
 if(ok)*id=next;else unlink(tmp);give();return ok;
}

#ifndef SSTV_HOST_TEST
static bool store_in_use(void){
 if(open_readers)return true;
#ifdef FMO_SSTV_STREAMING
 if(stream_file||stream_prepared||stream_complete)return true;
#endif
 return false;
}
const char *sstv_store_select(bool sd){
 if(sd&&!sstv_sd_ready())return "TF 卡未就绪，请查看卡管理";
 if(!take())return "相册忙，请稍后重试";
 if(store_in_use()){give();return "请先退出相册和 SSTV 接收或发射";}
 char previous[256];snprintf(previous,sizeof(previous),"%s",directory);const char *old_error=error;bool old_sd=on_sd,old_init=needs_initialization;
 force_flash=!sd;const char *result=sstv_store_init(NULL);force_flash=false;
 if(result){snprintf(directory,sizeof(directory),"%s",previous);error=old_error;on_sd=old_sd;needs_initialization=old_init;}
 if(!result)atomic_fetch_add(&storage_revision,1);
 give();return result;
}
const char *sstv_store_card_manage(bool format){
 if(!sstv_sd_supported())return "本设备没有 TF 卡接口";
 if(!take())return "相册忙，请稍后重试";
 if(store_in_use()){give();return "请先退出相册和 SSTV 接收或发射";}
 bool was_sd=on_sd,ok=sstv_sd_manage(format);
 if(was_sd){if(ok){snprintf(directory,sizeof(directory),"%s",sstv_sd_mount());error=NULL;}else error="TF 卡未就绪，请重新检测或切换内部存储";}
 atomic_fetch_add(&storage_revision,1);
 give();return ok?NULL:sstv_sd_error();
}
#else
const char *sstv_store_select(bool sd){(void)sd;return "Host test";}
const char *sstv_store_card_manage(bool format){(void)format;return "Host test";}
#endif
