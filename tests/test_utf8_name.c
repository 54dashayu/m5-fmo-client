#include <assert.h>
#include <string.h>
#include "utf8_name.h"
static bool supported(uint32_t c){return c<0x10000;}
int main(void){
 const char *names[]={"西南集群（渝）主节点","重庆小面 / BI4TES","臺灣電臺", "鸡婶婶的测试台"};
 char out[65];for(unsigned i=0;i<4;i++){assert(name_utf8_valid(names[i]));name_for_display(out,sizeof(out),names[i],supported);assert(!strcmp(out,names[i]));}
 assert(!name_utf8_valid("\xe4\xb8"));assert(!name_utf8_valid("\xc0\xaf"));assert(!name_utf8_valid("\xed\xa0\x80"));assert(!name_utf8_valid("bad\nname"));
 name_for_display(out,5,"重庆小面",supported);assert(!strcmp(out,"重"));
 name_for_display(out,sizeof(out),"电台📻",supported);assert(!strcmp(out,"电台?"));
 return 0;
}
