#include <assert.h>
#include "call_history.h"
int main(void){char a[13]={0},b[13]={0};call_history_push(a,b,"");assert(!a[0]);call_history_push(a,b,"BH1JSS");assert(!strcmp(a,"BH1JSS")&&!b[0]);for(int i=0;i<20;i++)call_history_push(a,b,"BH1JSS");assert(!b[0]);call_history_push(a,b,"BI4TES");assert(!strcmp(a,"BI4TES")&&!strcmp(b,"BH1JSS"));call_history_push(a,b,"BH1JSS");assert(!strcmp(a,"BH1JSS")&&!strcmp(b,"BI4TES"));call_history_push(a,b,"ABCDEFGHIJKL");assert(strlen(a)==12&&!strcmp(b,"BH1JSS"));return 0;}
