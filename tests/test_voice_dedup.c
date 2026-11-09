#include <assert.h>
#include "voice_dedup.h"
int main(void){voice_dedup_t s={0};bool same;
 assert(!voice_duplicate(&s,12,1000,1000,123,752,&same)&&!same);
 assert(voice_duplicate(&s,12,1000,1000,123,752,&same)&&!same);
 /* Distinct audio within one coarse timestamp must all reach the decoder. */
 for(unsigned c=124;c<171;c++)assert(!voice_duplicate(&s,12,1000,1000,c,752,&same)&&same);
 assert(!voice_duplicate(&s,12,1000,1000,170,1096,&same)&&same);
 assert(!voice_duplicate(&s,13,1000,1000,170,1096,&same)&&!same);
 assert(!voice_duplicate(&s,13,2000,1000,170,1096,&same)&&!same);
 assert(!voice_duplicate(&s,13,2000,1001,170,1096,&same)&&!same);
 return 0;}
