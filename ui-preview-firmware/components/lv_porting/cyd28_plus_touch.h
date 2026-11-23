#pragma once
#include <stdbool.h>
#include <stdint.h>
// Three-point affine calibration supports swapped and reversed sensor axes.
static inline bool cyd_map(const int32_t c[6],int rx,int ry,int *x,int *y){
 for(unsigned i=0;i<6;i++)if(c[i]<100||c[i]>4000)return false;
 int64_t ax=c[2]-c[0],ay=c[3]-c[1],bx=c[4]-c[0],by=c[5]-c[1];
 int64_t det=ax*by-ay*bx;if(det<100000&&det>-100000)return false;
 int64_t dx=rx-c[0],dy=ry-c[1];
 *x=30+(int)(260*(dx*by-dy*bx)/det);*y=30+(int)(150*(ax*dy-ay*dx)/det);
 return *x>=0&&*x<320&&*y>=0&&*y<240;
}
// Return one of four bottom actions, rejecting wake presses, drags and short noise.
static inline int cyd_nav_action(int sx,int sy,int x,int y,int64_t held_us,bool wake){
 if(wake||held_us<60000||sy<210||y<210||sy>=240||y>=240||sx<0||sx>=320||x<0||x>=320)return -1;
 if(x-sx>20||sx-x>20||y-sy>20||sy-y>20||x/80!=sx/80)return -1;
 return x/80;
}
bool cyd_touch_poll(int *x,int *y,bool *down);
void cyd_touch_recalibrate(void);

// Touch targets share the rendered list/carousel geometry. Blank rows are inert.
enum {CYD_CONTENT_NONE,CYD_CONTENT_HOME,CYD_CONTENT_LIST,CYD_CONTENT_GALLERY,CYD_CONTENT_ZOOM};
static inline int cyd_target(int kind,int rows,int x,int y){
 if(x<0||x>=320||y<0||y>=240)return -1;
 if(y>=210){int slot=x/80;return slot==3?30:slot==2&&kind!=CYD_CONTENT_HOME?3:slot;} /* rightmost bottom PTT; other pages retain Back */
 if(kind==CYD_CONTENT_LIST&&x>=12&&x<308&&y>=70&&y<70+rows*27)return 10+(y-70)/27;
 if(kind==CYD_CONTENT_ZOOM&&x>=40&&x<280&&y>=30&&y<210)return 21;
 if(kind==CYD_CONTENT_GALLERY){
  if(x>=12&&x<92&&y>=90&&y<150)return 20;
  if(x>=100&&x<220&&y>=75&&y<165)return 21;
  if(x>=228&&x<308&&y>=90&&y<150)return 22;
 }
 return -1;
}
static inline int cyd_tap(int kind,int rows,int sx,int sy,int x,int y,int64_t us,bool cancelled){
 if(cancelled||us<60000||x-sx>20||sx-x>20||y-sy>20||sy-y>20)return -1;
 int a=cyd_target(kind,rows,sx,sy),b=cyd_target(kind,rows,x,y);return a==b?a:-1;
}
static inline int cyd_page_start(int selected,int total,int per,int direction){
 if(total<=0||per<=0)return 0;
 int pages=(total+per-1)/per;return ((selected/per+pages+direction)%pages)*per;
}
