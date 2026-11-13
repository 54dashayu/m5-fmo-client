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

// Captions are indexed by physical position, matching the input mapping.
static inline const char*key_layout_caption(unsigned scheme,unsigned physical,int latch,int tx){
 if(scheme>=KEY_LAYOUT_COUNT)scheme=0;
 if(physical>=3)return "";
 unsigned role=key_layouts[scheme][physical];
 if(role==0)return "音量-";
 if(role==2)return "音量+";
 return latch?(tx?"停止发话":"开始发话"):"按住发话";
}
