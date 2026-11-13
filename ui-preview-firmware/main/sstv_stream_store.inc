/* Full-resolution RGB565 spool: two scanlines at a time, no frame-sized RAM. */
#ifdef FMO_SSTV_STREAMING
static FILE *stream_file;
static unsigned stream_rows_written;
static uint32_t stream_crc,stream_id;
static bool stream_prepared,stream_complete;
static char stream_context[128];
static void pending_path(char p[300]){snprintf(p,300,"%s/fmo-sstv-pending.tmp",directory);}
static void discard_locked(void){if(stream_file)fclose(stream_file);stream_file=NULL;stream_complete=false;stream_rows_written=0;char p[300];pending_path(p);unlink(p);}
void sstv_stream_discard(void){if(error||!take())return;discard_locked();stream_prepared=false;give();}
bool sstv_stream_prepare(void){
 if(error||!take())return false;
 sstv_entry_t entries[SSTV_GALLERY_MAX];int n=list(entries);sstv_storage_t space;
 bool ok=storage_stats(&space,entries,n)&&n<SSTV_GALLERY_MAX&&space.used+SSTV_BYTES*5/4+4096<=space.safe_limit;
 if(ok){stream_id=n?entries[0].id+1:1;ok=stream_id!=0;}
 stream_prepared=ok;if(!ok)save_error="相册空间不足，请先导出并删除旧图";give();return ok;
}
bool sstv_stream_begin(const char *relay){
 if(error||!take())return false;
 bool ok=false;
 discard_locked();
 if(stream_prepared){char p[300];pending_path(p);stream_file=fopen(p,"wb");
  if(stream_file){setvbuf(stream_file,NULL,_IONBF,0);uint8_t h[24]={0};memcpy(h,"SSTV565\2",8);put(h+16,SSTV_BYTES);memset(stream_context,0,sizeof(stream_context));if(relay)snprintf(stream_context,sizeof(stream_context),"%s",relay);
   ok=fwrite(h,1,24,stream_file)==24&&fwrite(stream_context,1,128,stream_file)==128;stream_crc=crc_update(~0u,(const uint8_t*)stream_context,128);}
 }
 if(!ok){discard_locked();save_error="图片临时保存失败";}give();return ok;
}
bool sstv_stream_rows(unsigned first,const uint8_t *pixels){
 if(error||!pixels||!take())return false;
 bool ok=stream_file&&first==stream_rows_written&&first<240&&fwrite(pixels,1,1280,stream_file)==1280;
 if(ok){stream_crc=crc_update(stream_crc,pixels,1280);stream_rows_written+=2;}else{discard_locked();save_error="图片写入失败";}
 give();return ok;
}
bool sstv_stream_finish(uint32_t at){
 if(error||!take())return false;
 bool ok=stream_file&&stream_rows_written==240;
 if(ok){uint8_t h[24]={0};memcpy(h,"SSTV565\2",8);put(h+12,at);put(h+16,SSTV_BYTES);put(h+20,~stream_crc);ok=fseek(stream_file,0,SEEK_SET)==0&&fwrite(h,1,24,stream_file)==24&&fflush(stream_file)==0&&fsync(fileno(stream_file))==0;}
 if(stream_file){if(fclose(stream_file))ok=false;stream_file=NULL;}
 stream_complete=ok;if(!ok)save_error="图片保存未完成";give();return ok;
}
bool sstv_stream_finish_partial(uint32_t at){
 if(error||!take())return false;
 if(stream_complete){give();return true;}
 unsigned received=stream_rows_written;bool ok=stream_file&&received>0&&received<240;
 if(ok){
  uint8_t zero[1280]={0};
  while(ok&&stream_rows_written<240){ok=fwrite(zero,1,sizeof(zero),stream_file)==sizeof(zero);if(ok)stream_rows_written+=2;}
  if(ok)ok=fflush(stream_file)==0;
  if(stream_file){if(fclose(stream_file))ok=false;stream_file=NULL;}
  char path[300];pending_path(path);FILE*f=ok?fopen(path,"r+b"):NULL;ok=ok&&f;
  if(f){setvbuf(f,NULL,_IONBF,0);uint8_t h[24],buf[1280];ok=fread(h,1,24,f)==24;
   stream_context[120]=0;put((uint8_t*)stream_context+124,received);
   uint32_t sum=crc_update(~0u,(const uint8_t*)stream_context,128);
   if(ok)ok=fseek(f,152,SEEK_SET)==0;
   for(size_t left=SSTV_BYTES;ok&&left;){size_t n=left>sizeof(buf)?sizeof(buf):left;ok=fread(buf,1,n,f)==n;if(ok)sum=crc_update(sum,buf,n);left-=n;}
   h[7]|=128;put(h+12,at);put(h+20,~sum);
   if(ok)ok=fseek(f,0,SEEK_SET)==0&&fwrite(h,1,24,f)==24&&fwrite(stream_context,1,128,f)==128&&fflush(f)==0&&fsync(fileno(f))==0;
   if(fclose(f))ok=false;
  }
 }
 stream_complete=ok;if(!ok)save_error="残图保存失败";give();return ok;
}
bool sstv_stream_save(uint32_t *id){
 if(error||!id||!take())return false;
 bool ok=false;
 if(stream_complete&&stream_id){char p[300],target[300];pending_path(p);filename(target,stream_id);FILE*f=fopen(p,"r+b");
  if(f){uint8_t v[4];put(v,stream_id);ok=fseek(f,8,SEEK_SET)==0&&fwrite(v,1,4,f)==4&&fflush(f)==0&&fsync(fileno(f))==0;if(fclose(f))ok=false;}
  if(ok)ok=rename(p,target)==0;
  if(ok){*id=stream_id;stream_complete=false;stream_prepared=false;}
 }
 give();return ok;
}
static bool export_locked(uint32_t id,sstv_stream_writer_t writer,void *ctx){
 char path[300];filename(path,id);FILE*f=fopen(path,"rb");if(!f)return false;setvbuf(f,NULL,_IONBF,0);
 uint8_t h[24],buf[1280];bool ok=header(f,h)&&get(h+8)==id&&(h[7]&127)!=3;uint32_t sum=~0u;
 if(ok&&h[7]>=2){ok=fread(buf,1,128,f)==128;if(ok)sum=crc_update(sum,buf,128);}
 size_t size=ok?get(h+16):0;long start=ftell(f);
 for(size_t left=size;ok&&left;){size_t n=left>sizeof(buf)?sizeof(buf):left;ok=fread(buf,1,n,f)==n;if(ok)sum=crc_update(sum,buf,n);left-=n;}
 ok=ok&&fgetc(f)==EOF&&~sum==get(h+20);
 if(ok)ok=fseek(f,start,SEEK_SET)==0;
 for(size_t left=size;ok&&left;){size_t n=left>sizeof(buf)?sizeof(buf):left;ok=fread(buf,1,n,f)==n&&writer(ctx,buf,n);left-=n;}
 fclose(f);return ok;
}
bool sstv_store_export(uint32_t id,sstv_stream_writer_t writer,void *ctx){if(error||!writer||!take())return false;
 bool ok=export_locked(id,writer,ctx);give();return ok;}
typedef struct{uint8_t *pixels;size_t offset;bool high;} preview_context_t;
static bool preview_write(void *arg,const uint8_t *data,size_t n){
 preview_context_t*c=arg;
 unsigned width=c->high?320:160,height=c->high?240:120,bpp=c->high?2:1;
 for(size_t i=0;i<n;i+=bpp){
  unsigned p=(c->offset+i)/bpp,y=p/width,x=p%width;
  unsigned dy=(y*SSTV_PREVIEW_HEIGHT+height-1)/height,dx=(x*SSTV_PREVIEW_WIDTH+width-1)/width;
  if(dy>=SSTV_PREVIEW_HEIGHT||dx>=SSTV_PREVIEW_WIDTH||dy*height/SSTV_PREVIEW_HEIGHT!=y||dx*width/SSTV_PREVIEW_WIDTH!=x)continue;
  unsigned color=data[i];if(c->high){unsigned v=data[i]|data[i+1]<<8,r=(v>>11)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;color=(r&224)|((g>>3)&28)|(b>>6);}
  c->pixels[dy*SSTV_PREVIEW_WIDTH+dx]=color;
 }
 c->offset+=n;return true;
}
bool sstv_store_preview(uint32_t id,uint8_t *indexed){
 if(error||!indexed||!take())return false;
 char path[300];filename(path,id);uint8_t h[24];FILE*f=fopen(path,"rb");bool ok=f&&header(f,h);if(f)fclose(f);
 if(ok){preview_context_t ctx={.pixels=indexed,.high=get(h+16)!=19200};ok=export_locked(id,preview_write,&ctx);}
 give();return ok;
}
#endif
