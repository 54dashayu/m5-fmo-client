#pragma once
#include <string.h>
// Consecutive packets or repeated calls from the same station occupy one entry.
static inline void call_history_push(char recent[13],char previous[13],const char*call){
 if(!call||!call[0]||!strncmp(recent,call,12))return;
 memcpy(previous,recent,13);size_t n=strlen(call);if(n>12)n=12;
 memcpy(recent,call,n);recent[n]=0;
}
