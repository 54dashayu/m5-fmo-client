#pragma once
#include <stdint.h>
#define KEY_LAYOUT_COUNT 6
// Physical A/B/C: left/middle/right when the three keys are on top.
// Entries contain logical roles: 0=minus/back, 1=PTT/confirm, 2=plus/menu.
static const uint8_t key_layouts[KEY_LAYOUT_COUNT][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static inline uint8_t key_layout_mask(unsigned scheme,uint8_t physical){
 if(scheme>=KEY_LAYOUT_COUNT)scheme=0;
 uint8_t logical=0;for(unsigned i=0;i<3;i++)if(physical&(1u<<i))logical|=1u<<key_layouts[scheme][i];return logical;
}
static inline char key_layout_letter(unsigned scheme,unsigned role){
 if(scheme>=KEY_LAYOUT_COUNT)scheme=0;
 for(unsigned i=0;i<3;i++)if(key_layouts[scheme][i]==role)return 'A'+i;
 return '?';
}
