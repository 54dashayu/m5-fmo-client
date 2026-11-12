#include "battery.h"
#include "side_leds.h"
#include <string.h>
void battery_init(void) {}
void battery_view(int *percent,bool *charging){*percent=-1;*charging=false;}
void side_leds_init(void) {}
void side_leds_update(bool tx,bool rx,int64_t ms){(void)tx;(void)rx;(void)ms;}
void side_leds_view(side_leds_view_t *out){memset(out,0,sizeof(*out));}
