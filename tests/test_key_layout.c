#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "key_layout.h"
#include "buttons.h"
static int starts,stops,shorts,last_key;
static void event(int key,key_action_t action,void*ctx){(void)ctx;last_key=key;if(action==KEY_PTT_START)starts++;if(action==KEY_PTT_STOP)stops++;if(action==KEY_SHORT)shorts++;}
int main(void){
 for(unsigned scheme=0;scheme<6;scheme++){
  unsigned seen=0;for(unsigned physical=0;physical<3;physical++){unsigned role=key_layouts[scheme][physical];seen|=1<<role;assert(key_layout_letter(scheme,role)==(char)('A'+physical));assert(key_layout_mask(scheme,1<<physical)==(1<<role));}
  assert(seen==7);assert(key_layout_mask(scheme,7)==7);
  for(unsigned physical=0;physical<3;physical++){
   unsigned role=key_layouts[scheme][physical];
   const char*expected=role==0?"音量-":role==2?"音量+":"按住发话";
   assert(!strcmp(key_layout_caption(scheme,physical,0,0),expected));
   assert(!strcmp(key_layout_caption(scheme,physical,1,0),role==1?"开始发话":expected));
   assert(!strcmp(key_layout_caption(scheme,physical,1,1),role==1?"停止发话":expected));
   buttons_t b;starts=stops=shorts=0;buttons_init(&b,0,event,NULL);buttons_set_momentary(&b,true);buttons_step(&b,0,0,true);
   uint8_t p=key_layout_mask(scheme,1<<physical);buttons_step(&b,p,1000,true);buttons_step(&b,p,1031,true);buttons_step(&b,0,1100,true);buttons_step(&b,0,1131,true);
   if(key_layouts[scheme][physical]==1)assert(starts==1&&stops==1&&shorts==0);else assert(starts==0&&stops==0&&shorts==1&&last_key==key_layouts[scheme][physical]);
  }
 }
 assert(key_layout_mask(999,2)==2);puts("PASS all six physical mappings, correct PTT edge and release, volume roles, invalid fallback");
}
