#include <assert.h>
#include <stdio.h>
#include "auto_ca_policy.h"
int main(void){
 bool valid[11]={0};char roots[11][44]={{0}},accepted[10][44]={{0}};
 strcpy(roots[1],"BH");strcpy(roots[2],"TES");valid[1]=valid[2]=true;
 strcpy(accepted[0],"TES");assert(auto_ca_choose(3,1,valid,roots,accepted,1)==2);
 strcpy(accepted[1],"BH");assert(auto_ca_choose(3,1,valid,roots,accepted,2)==1);assert(auto_ca_choose(3,2,valid,roots,accepted,2)==2);
 assert(auto_ca_choose(1,1,valid,roots,accepted,1)==0);valid[2]=false;assert(auto_ca_choose(3,1,valid,roots,accepted,1)==0);
 valid[2]=true;assert(auto_ca_choose(3,1,valid,roots,accepted,0)==0);assert(auto_ca_choose(3,0,valid,roots,accepted,1)==2);
 assert(!auto_ca_accepts(accepted,11,"TES"));assert(!auto_ca_accepts(accepted,1,""));
 puts("PASS current identity priority, enabled-bank filtering, invalid certificates, unknown metadata and multiple-root matching");
}
