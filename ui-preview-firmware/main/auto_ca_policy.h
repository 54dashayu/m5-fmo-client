#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define AUTO_CA_MAX 10
static inline bool auto_ca_accepts(const char accepted[AUTO_CA_MAX][44],int count,const char*root){
 if(!root||!root[0]||count<0||count>AUTO_CA_MAX)return false;
 for(int i=0;i<count;i++)if(!strcmp(accepted[i],root))return true;
 return false;
}
// Keep a compatible current identity. Otherwise choose the first enabled valid bank.
static inline int auto_ca_choose(uint32_t enabled,int current,const bool valid[AUTO_CA_MAX+1],const char roots[AUTO_CA_MAX+1][44],const char accepted[AUTO_CA_MAX][44],int count){
 if(current>0&&current<=AUTO_CA_MAX&&valid[current]&&auto_ca_accepts(accepted,count,roots[current]))return current;
 for(int i=1;i<=AUTO_CA_MAX;i++)if((enabled&(1u<<(i-1)))&&valid[i]&&auto_ca_accepts(accepted,count,roots[i]))return i;
 return 0;
}
