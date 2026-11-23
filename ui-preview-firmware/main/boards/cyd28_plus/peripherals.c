#include "battery.h"
#include "side_leds.h"
#include "driver/gpio.h"
#include <string.h>
// No uncalibrated battery percentage; no M5 I2C/LED peripherals on this board.
void battery_init(void) {}
void battery_view(int *p,bool *c){*p=-1;*c=false;}
void side_leds_init(void){
 const int pins[]={4,22,16,17};
 for(unsigned i=0;i<4;i++){gpio_config_t cfg={.pin_bit_mask=1ULL<<pins[i],.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&cfg));gpio_set_level(pins[i],pins[i]==4?0:1);}
 // Restore dev5: GPIO4 LOW enables 8002B, GPIO26 is DAC2.
 gpio_set_level(4,0);
}
void side_leds_update(bool tx,bool rx,int64_t ms){(void)tx;(void)rx;(void)ms;}
void side_leds_view(side_leds_view_t *v){memset(v,0,sizeof(*v));}
