#include "station_tx.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <fcntl.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static station_tx_prepare_fn prepare;
static bool(*is_busy)(void);
static atomic_bool enabled,immediate;
static atomic_uint generation;
static TaskHandle_t task;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static char message[96]="广播已关闭";
static unsigned sent,failed;
static time_t last_sent;
static void status(const char*s){portENTER_CRITICAL(&mux);snprintf(message,sizeof(message),"%s",s);portEXIT_CRITICAL(&mux);}
static bool live(unsigned g){return atomic_load(&enabled)&&atomic_load(&generation)==g&&!is_busy();}
static bool ready(int fd,bool write,int64_t end,unsigned g){while(esp_timer_get_time()<end&&live(g)){fd_set f;FD_ZERO(&f);FD_SET(fd,&f);struct timeval tv={.tv_usec=100000};int n=select(fd+1,write?NULL:&f,write?&f:NULL,NULL,&tv);if(n>0)return true;if(n<0&&errno!=EINTR)return false;}return false;}
static bool send_all(int fd,const char*s,int64_t end,unsigned g){size_t n=strlen(s);while(n){if(!ready(fd,true,end,g))return false;int k=send(fd,s,n,0);if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))continue;if(k<=0)return false;n-=k;s+=k;}return true;}
static bool line(int fd,char out[256],int64_t end,unsigned g){size_t n=0;while(n<255&&ready(fd,false,end,g)){char c;int k=recv(fd,&c,1,0);if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))continue;if(k<=0)return false;if(c=='\n'){out[n]=0;return true;}if(c!='\r')out[n++]=c;}return false;}
// Require the exact login and the exact token 'verified'; 'unverified' is rejected.
static bool verified(const char*s,const char*login){char call[16],state[24];return sscanf(s,"# logresp %15s %23[^,]",call,state)==2&&!strcmp(call,login)&&!strcmp(state,"verified");}
static bool transmit(station_tx_packet_t*p,unsigned g){
 struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*a=NULL;char port[8];snprintf(port,sizeof(port),"%d",p->port);status("正在连接 APRS-IS…");
 if(getaddrinfo(p->host,port,&hints,&a)||!a){status("APRS-IS 域名解析失败，稍后重试");return false;}
 int fd=-1;bool ok=false;int64_t end=esp_timer_get_time()+12000000;
 if(!live(g))goto done;
 fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(fd<0)goto done;fcntl(fd,F_SETFL,O_NONBLOCK);
 int c=connect(fd,a->ai_addr,a->ai_addrlen),error=0;socklen_t size=sizeof(error);
 if(c!=0&&errno!=EINPROGRESS)goto done;
 if(!ready(fd,true,end,g)||getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)||error)goto done;
 char response[256],login[128];if(!line(fd,response,end,g)||response[0]!='#')goto done;
 snprintf(login,sizeof(login),"user %s pass %d vers FMO-ESP32 " FMO_VERSION "\r\n",p->login,p->passcode);
 if(!send_all(fd,login,end,g)){memset(login,0,sizeof(login));goto done;}memset(login,0,sizeof(login));
 bool accepted=false;for(int i=0;i<20&&line(fd,response,end,g);i++){if(!strncmp(response,"# logresp ",10)){accepted=verified(response,p->login);break;}}
 if(!accepted){status("APRS 登录未验证，请检查登录码");goto done;}
 if(!live(g))goto done;
 ok=send_all(fd,p->line,end,g);
 if(ok)status("已交给 APRS-IS；公开地图显示尚未确认");
 done:if(fd>=0)close(fd);freeaddrinfo(a);return ok;
}
static void worker(void*unused){(void)unused;int64_t due=0,last_attempt=0;
 for(;;){vTaskDelay(pdMS_TO_TICKS(1000));if(!atomic_load(&enabled))continue;int64_t now=esp_timer_get_time();if(is_busy()){status("通联或 SSTV 接收中，广播等待空闲");continue;}
 bool requested=atomic_load(&immediate);if(now<due&&!requested)continue;if(last_attempt&&now-last_attempt<60000000)continue;
 atomic_exchange(&immediate,false);unsigned g=atomic_load(&generation);station_tx_packet_t p={0};char reason[96]={0};
 if(!prepare(&p,reason)){status(reason);due=now+30000000;continue;}
 if(!live(g)){memset(&p,0,sizeof(p));continue;}
 last_attempt=now;bool ok=transmit(&p,g);memset(&p,0,sizeof(p));
 portENTER_CRITICAL(&mux);if(ok){sent++;last_sent=time(NULL);}else failed++;portEXIT_CRITICAL(&mux);
 due=esp_timer_get_time()+(ok?600000000:60000000);
 if(!live(g))status(atomic_load(&enabled)?"广播已暂停，将按最新设置重试":"广播已关闭");
 else if(!ok){portENTER_CRITICAL(&mux);if(!strcmp(message,"正在连接 APRS-IS…"))snprintf(message,sizeof(message),"连接或发送失败，60 秒后重试");portEXIT_CRITICAL(&mux);}
 }
}
void station_tx_init(station_tx_prepare_fn p,bool(*b)(void)){prepare=p;is_busy=b;}
bool station_tx_update(bool on,bool now){atomic_store(&enabled,on);atomic_fetch_add(&generation,1);atomic_store(&immediate,on&&now);if(!on){status("广播已关闭");return true;}if(!task&&xTaskCreate(worker,"station_tx",6144,NULL,2,&task)!=pdPASS){task=NULL;atomic_store(&enabled,false);status("内存不足，广播未启动");return false;}status("等待联网、证书与空闲时段");return true;}
void station_tx_status(cJSON*j){char s[96];unsigned n,f;time_t t;portENTER_CRITICAL(&mux);snprintf(s,sizeof(s),"%s",message);n=sent;f=failed;t=last_sent;portEXIT_CRITICAL(&mux);cJSON_AddStringToObject(j,"broadcastStatus",s);cJSON_AddNumberToObject(j,"broadcastSent",n);cJSON_AddNumberToObject(j,"broadcastFailed",f);cJSON_AddNumberToObject(j,"broadcastLastSent",t);}
