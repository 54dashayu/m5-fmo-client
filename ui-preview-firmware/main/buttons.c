#include "buttons.h"
#include <string.h>
static void emit(buttons_t*b,int key,key_action_t a){b->callback(key,a,b->context);}
void buttons_init(buttons_t*b,uint8_t p,key_callback_t cb,void*ctx){memset(b,0,sizeof(*b));b->raw=b->stable=p;b->blocked=p!=0;b->callback=cb;b->context=ctx;}
void buttons_block(buttons_t*b){b->blocked=true;if(b->ptt){b->ptt=false;emit(b,1,KEY_PTT_STOP);}}
void buttons_set_momentary(buttons_t*b,bool enabled){buttons_block(b);b->momentary=enabled;}
void buttons_step(buttons_t*b,uint8_t p,uint32_t now,bool home){for(int i=0;i<3;i++){uint8_t bit=1<<i;if((p^b->raw)&bit){b->raw^=bit;b->changed[i]=now;}if(((b->raw^b->stable)&bit)&&now-b->changed[i]>=30){bool down=p&bit;b->stable^=bit;if(down){b->down[i]=now;b->held[i]=false;}else{if(i==1&&b->ptt){b->ptt=false;emit(b,1,KEY_PTT_STOP);}if(!b->blocked&&!b->held[i]&&!(home&&i==1&&b->momentary)){if(home&&i==1){if(now-b->last_b>=350){b->last_b=now;emit(b,i,KEY_SHORT);}}else emit(b,i,KEY_SHORT);}}}}
 if(b->stable&&(b->stable&(b->stable-1))){if(!b->blocked)emit(b,-1,KEY_CANCEL);buttons_block(b);}if(b->blocked){if(!b->stable&&!p)b->blocked=false;return;}
 if(home&&b->momentary&&b->stable==2&&!b->held[1]){b->held[1]=true;b->ptt=true;emit(b,1,KEY_PTT_START);}
 for(int i=0;i<3;i++)if((b->stable&(1<<i))&&!b->held[i]){uint32_t delay=800;if(now-b->down[i]>=delay){b->held[i]=true;emit(b,i,KEY_LONG);if(b->blocked)break;}}
}
