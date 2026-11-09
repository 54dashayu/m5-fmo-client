#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Reject malformed sequences, overlong encodings, surrogates and controls. */
static inline size_t name_codepoint(const unsigned char *s,uint32_t *cp){
 if(!s[0])return 0;
 unsigned n;uint32_t v,min;
 if(s[0]<0x80){*cp=s[0];return s[0]>=32?1:0;}
 if(s[0]>=0xc2&&s[0]<=0xdf){n=2;v=s[0]&31;min=0x80;}
 else if(s[0]>=0xe0&&s[0]<=0xef){n=3;v=s[0]&15;min=0x800;}
 else if(s[0]>=0xf0&&s[0]<=0xf4){n=4;v=s[0]&7;min=0x10000;}else return 0;
 for(unsigned i=1;i<n;i++){if((s[i]&0xc0)!=0x80)return 0;v=(v<<6)|(s[i]&63);}
 if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff)||(v>=0x7f&&v<=0x9f))return 0;
 *cp=v;return n;
}
static inline bool name_utf8_valid(const char *s){while(*s){uint32_t cp;size_t n=name_codepoint((const unsigned char*)s,&cp);if(!n)return false;s+=n;}return true;}
/* Display-only replacement; stored name remains intact. Never split UTF-8. */
static inline void name_for_display(char *out,size_t capacity,const char *src,bool (*supported)(uint32_t)){
 size_t used=0;if(!capacity)return;
 while(*src){uint32_t cp;size_t n=name_codepoint((const unsigned char*)src,&cp);if(!n){if(used+1<capacity)out[used++]='?';break;}
 bool ok=supported(cp);size_t bytes=ok?n:1;if(used+bytes>=capacity)break;
 if(ok)for(size_t i=0;i<n;i++)out[used++]=src[i];else out[used++]='?';src+=n;
 }out[used]=0;
}
