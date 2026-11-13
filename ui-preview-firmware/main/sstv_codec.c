#include "sstv_codec.h"
#include "esp_jpeg_enc.h"
#include "esp_jpeg_dec.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#define PIXELS (320*240*2)
#define JPEG_LIMIT (160*1024)
bool sstv_jpeg_encode(const uint8_t*p,uint8_t**out,size_t*n){
 *out=NULL;*n=0;
 uint8_t*input=heap_caps_aligned_alloc(16,PIXELS,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 uint8_t*encoded=heap_caps_malloc(JPEG_LIMIT,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!input||!encoded){free(input);free(encoded);return false;}memcpy(input,p,PIXELS);
 jpeg_enc_config_t cfg=DEFAULT_JPEG_ENC_CONFIG();cfg.width=320;cfg.height=240;cfg.src_type=JPEG_PIXEL_FORMAT_RGB565_LE;cfg.subsampling=JPEG_SUBSAMPLE_444;cfg.quality=90;cfg.task_enable=false;
 jpeg_enc_handle_t enc=NULL;int bytes=0;bool ok=jpeg_enc_open(&cfg,&enc)==JPEG_ERR_OK;
 if(ok)ok=jpeg_enc_process(enc,input,PIXELS,encoded,JPEG_LIMIT,&bytes)==JPEG_ERR_OK&&bytes>0&&bytes<JPEG_LIMIT;
 if(enc){jpeg_enc_close(enc);}free(input);
 if(!ok){free(encoded);return false;}*out=encoded;*n=(size_t)bytes;return true;
}
bool sstv_jpeg_decode(const uint8_t*jpeg,size_t n,uint8_t*p){
 if(!n||n>JPEG_LIMIT)return false;
 uint8_t*aligned=heap_caps_aligned_alloc(16,PIXELS,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!aligned)return false;
 jpeg_dec_config_t cfg=DEFAULT_JPEG_DEC_CONFIG();cfg.output_type=JPEG_PIXEL_FORMAT_RGB565_LE;
 jpeg_dec_handle_t dec=NULL;jpeg_dec_header_info_t info={0};jpeg_dec_io_t io={.inbuf=(uint8_t*)jpeg,.inbuf_len=(int)n,.outbuf=aligned};
 bool ok=jpeg_dec_open(&cfg,&dec)==JPEG_ERR_OK;
 if(ok)ok=jpeg_dec_parse_header(dec,&io,&info)==JPEG_ERR_OK&&info.width==320&&info.height==240;
 if(ok)ok=jpeg_dec_process(dec,&io)==JPEG_ERR_OK&&io.out_size==PIXELS;
 if(ok){memcpy(p,aligned,PIXELS);}if(dec){jpeg_dec_close(dec);}free(aligned);return ok;
}
