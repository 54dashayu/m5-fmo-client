#include "cyd28_plus_touch.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 const int32_t normal[]={400,500,3600,500,400,3500};
 const int32_t swapped[]={3500,3600,3500,400,500,3600};
 const int32_t skew[]={400,500,3600,600,500,3500};
 const int32_t bad[]={100,100,100,100,100,100};
 const int32_t *cases[]={normal,swapped,skew};
 for(unsigned i=0;i<3;i++){int x,y;const int32_t*c=cases[i];assert(cyd_map(c,c[0],c[1],&x,&y)&&x==30&&y==30);assert(cyd_map(c,c[2],c[3],&x,&y)&&x==290&&y==30);assert(cyd_map(c,c[4],c[5],&x,&y)&&x==30&&y==180);assert(cyd_map(c,(c[2]+c[4])/2,(c[3]+c[5])/2,&x,&y)&&x==160&&y==105);}
 for(int k=0;k<4;k++)assert(cyd_nav_action(k*80+40,220,k*80+40,220,100000,false)==k);
 assert(cyd_nav_action(30,220,30,220,100000,true)==-1);assert(cyd_nav_action(30,220,30,220,10000,false)==-1);assert(cyd_nav_action(30,220,100,220,100000,false)==-1);assert(cyd_nav_action(30,190,30,220,100000,false)==-1);assert(cyd_nav_action(79,220,81,220,100000,false)==-1);
 for(int row=0;row<5;row++)assert(cyd_tap(CYD_CONTENT_LIST,5,150,75+row*27,150,75+row*27,100000,false)==10+row);
 assert(cyd_target(CYD_CONTENT_LIST,3,150,151)==-1);
 assert(cyd_target(CYD_CONTENT_LIST,5,0,75)==-1);
 assert(cyd_tap(CYD_CONTENT_LIST,5,150,96,150,97,100000,false)==-1);
 assert(cyd_tap(CYD_CONTENT_LIST,5,150,75,150,75,1000,false)==-1);
 assert(cyd_tap(CYD_CONTENT_LIST,5,150,75,150,75,100000,true)==-1);
 assert(cyd_target(CYD_CONTENT_GALLERY,0,50,120)==20);
 assert(cyd_target(CYD_CONTENT_GALLERY,0,160,120)==21);
 assert(cyd_target(CYD_CONTENT_GALLERY,0,270,120)==22);
 assert(cyd_target(CYD_CONTENT_GALLERY,0,95,120)==-1);
 assert(cyd_target(CYD_CONTENT_ZOOM,0,100,200)==21);
 assert(cyd_target(CYD_CONTENT_ZOOM,0,280,200)==-1);
 assert(cyd_target(CYD_CONTENT_LIST,5,280,225)==30);
 assert(cyd_page_start(0,7,4,-1)==4&&cyd_page_start(4,7,4,1)==0);
 assert(cyd_page_start(0,0,4,1)==0);
 assert(cyd_target(CYD_CONTENT_HOME,0,79,220)==0);
 assert(cyd_target(CYD_CONTENT_HOME,0,80,220)==1);
 assert(cyd_target(CYD_CONTENT_HOME,0,160,220)==2);
 assert(cyd_tap(CYD_CONTENT_HOME,0,79,220,80,220,100000,false)==-1);
 assert(cyd_target(CYD_CONTENT_HOME,0,240,220)==30);
 assert(cyd_target(CYD_CONTENT_LIST,5,200,220)==3);
 assert(cyd_target(CYD_CONTENT_LIST,5,280,29)==-1);
 int x,y;assert(!cyd_map(bad,100,100,&x,&y));assert(!cyd_map(normal,4095,4095,&x,&y));puts("PASS touch affine: normal, reversed/swapped, skew, midpoint, degenerate, bounds");
}
