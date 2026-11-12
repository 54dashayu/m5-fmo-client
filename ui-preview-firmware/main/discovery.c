#include "discovery.h"
#include "voice.h"
#include "cert_wire.h"
#include "utf8_name.h"
#include "esp_http_client.h"
#include "monocypher-ed25519.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/netdb.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

typedef struct {relay_t relay;time_t verified,attempted,seen;} candidate_t;
static candidate_t rows[DISCOVERY_MAX];
static uint32_t checking_uid;
static int count;static uint32_t revision=1;static time_t updated;
static SemaphoreHandle_t mutex,allocation_lock;
static atomic_bool refresh=true,owner_valid=false;
static discovery_identity_fn identity;
static char owner[65],message[96]="等待联网后自动发现";
static const char *str(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool same(const relay_t*a,const relay_t*b){return a->uid==b->uid&&a->port==b->port&&!strcmp(a->host,b->host)&&!strcmp(a->fingerprint,b->fingerprint)&&!strcmp(a->call,b->call);}
static void status(const char*s){xSemaphoreTake(mutex,portMAX_DELAY);snprintf(message,sizeof(message),"%s",s);xSemaphoreGive(mutex);}
static bool current_owner(char out[65]){char call[8];uint32_t uid;return identity(NULL,NULL,call,&uid,out);}
static bool fresh(candidate_t*c,time_t now){return now>=c->seen&&now-c->seen<1200&&now>=updated&&now-updated<600;}
static bool usable(candidate_t*c,time_t now){return c->verified>0&&now>=c->verified&&now-c->verified<3600&&fresh(c,now);}
static bool parse(const cJSON*j,candidate_t*c,time_t now){
 const char*n=str(j,"name"),*call=str(j,"callsign"),*host=str(j,"host"),*fp=str(j,"fingerprint");
 const cJSON*u=cJSON_GetObjectItemCaseSensitive(j,"uid"),*p=cJSON_GetObjectItemCaseSensitive(j,"port"),*seen=cJSON_GetObjectItemCaseSensitive(j,"lastSeen");uint8_t key[32];
 bool confirmed=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"adminConfirmed"))&&cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"mutualTrustConfirmed"));
 if((!confirmed&&(!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"rootTrusted"))||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"sourceVerified"))))||!n||!*n||strlen(n)>64||!name_utf8_valid(n)||!wire_callsign(call)||!host||!*host||strlen(host)>63||!wire_unb64(fp,key,32)||!cJSON_IsNumber(u)||u->valuedouble<1||u->valuedouble>4294967295.0||floor(u->valuedouble)!=u->valuedouble||!cJSON_IsNumber(p)||p->valuedouble<1||p->valuedouble>65535||floor(p->valuedouble)!=p->valuedouble||!cJSON_IsNumber(seen)||seen->valuedouble>now||now-seen->valuedouble>=1200)return false;
 for(const char*s=host;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='.'||*s=='-'))return false;
 for(const char*s=n;*s;s++)if((unsigned char)*s<32)return false;
 memset(c,0,sizeof(*c));snprintf(c->relay.name,65,"%s",n);snprintf(c->relay.call,8,"%s",call);snprintf(c->relay.host,64,"%s",host);snprintf(c->relay.fingerprint,44,"%s",fp);c->relay.uid=u->valuedouble;c->relay.port=p->valueint;c->seen=seen->valuedouble;return true;
}
static bool fetch_catalog(void){
 // Signed public catalog: no TLS allocation on the audio-constrained ESP32.
 esp_http_client_config_t cfg={.url="http://fmo.bh1jss.net/m5-trusted-discovery.json",.timeout_ms=2000,.buffer_size=512,.buffer_size_tx=512,.disable_auto_redirect=true};
 esp_http_client_handle_t h=esp_http_client_init(&cfg);bool ok=false;int used=0;char*body=NULL;
 if(h&&esp_http_client_open(h,0)==ESP_OK){int64_t n=esp_http_client_fetch_headers(h);if(esp_http_client_get_status_code(h)==200&&n>0&&n<=8192){body=calloc(1,n+1);if(body){while(used<n&&!voice_busy()){int k=esp_http_client_read(h,body+used,n-used);if(k<=0)break;used+=k;}ok=used==n;}}}
 if(h){esp_http_client_close(h);esp_http_client_cleanup(h);}
 cJSON*envelope=ok?cJSON_Parse(body):NULL;free(body);if(!envelope)return false;
 const char*payload=str(envelope,"payload"),*signature=str(envelope,"signature");uint8_t pk[32],sig[64];
 ok=payload&&strlen(payload)<=6144&&wire_unb64("D18B5J0btdUxLifmQn9peX-g4CZBZ0vh7jcWUnrhieA",pk,32)&&wire_unb64(signature,sig,64)&&crypto_ed25519_check(sig,pk,(const uint8_t*)payload,strlen(payload))==0;
 cJSON*j=ok?cJSON_Parse(payload):NULL;cJSON_Delete(envelope);if(!j)return false;
 const cJSON*a=cJSON_GetObjectItemCaseSensitive(j,"servers"),*t=cJSON_GetObjectItemCaseSensitive(j,"updatedAt");const char*f=str(j,"format");time_t now=time(NULL);
 ok=f&&!strcmp(f,"m5-trusted-discovery-v1")&&cJSON_IsArray(a)&&cJSON_GetArraySize(a)<=DISCOVERY_MAX&&cJSON_IsNumber(t)&&t->valuedouble<=now&&now-t->valuedouble<600&&cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"sourceConnected"));
 candidate_t*next=calloc(DISCOVERY_MAX,sizeof(candidate_t));if(!next)ok=false;int n=0;
 if(ok){const cJSON*v;cJSON_ArrayForEach(v,a){candidate_t c;if(parse(v,&c,now))next[n++]=c;}
 xSemaphoreTake(mutex,portMAX_DELAY);for(int i=0;i<n;i++)for(int k=0;k<count;k++)if(same(&next[i].relay,&rows[k].relay)){next[i].verified=rows[k].verified;next[i].attempted=rows[k].attempted;break;}
 memcpy(rows,next,n*sizeof(*next));count=n;updated=t->valuedouble;revision++;snprintf(message,sizeof(message),"可信目录节点 %d，接入后确认权限",n);xSemaphoreGive(mutex);}
 if(ok){const cJSON*v;cJSON_ArrayForEach(v,a){candidate_t c;if(parse(v,&c,now))relays_update_name(v);}}
 free(next);cJSON_Delete(j);return ok;
}
static bool same_broker(const relay_t*a,const relay_t*b){
 if(a->uid==b->uid||!strcmp(a->host,b->host))return true;
 struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*x=NULL,*y=NULL;bool equal=false;
 if(!getaddrinfo(a->host,NULL,&hints,&x)&&!getaddrinfo(b->host,NULL,&hints,&y))for(struct addrinfo*i=x;i;i=i->ai_next)for(struct addrinfo*k=y;k;k=k->ai_next)if(((struct sockaddr_in*)i->ai_addr)->sin_addr.s_addr==((struct sockaddr_in*)k->ai_addr)->sin_addr.s_addr)equal=true;
 if(x){freeaddrinfo(x);}
 if(y){freeaddrinfo(y);}
 return equal;
}
static bool public_target(const char*host){struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*a=NULL;bool ok=false;if(getaddrinfo(host,NULL,&hints,&a))return false;for(struct addrinfo*p=a;p;p=p->ai_next){uint32_t ip=ntohl(((struct sockaddr_in*)p->ai_addr)->sin_addr.s_addr);unsigned x=ip>>24,y=(ip>>16)&255;if(x==0||x==10||x==127||x>=224||(x==169&&y==254)||(x==172&&y>=16&&y<=31)||(x==192&&y==168)||(x==100&&y>=64&&y<=127)||(x==198&&(y==18||y==19))){ok=false;break;}ok=true;}freeaddrinfo(a);return ok;}
static bool socket_ready(int fd,bool writing,int64_t deadline){while(esp_timer_get_time()<deadline&&!voice_busy()){fd_set f;FD_ZERO(&f);FD_SET(fd,&f);struct timeval tv={.tv_usec=100000};int n=select(fd+1,writing?NULL:&f,writing?&f:NULL,NULL,&tv);if(n>0)return true;if(n<0&&errno!=EINTR)return false;}return false;}
static bool write_all(int fd,const uint8_t*p,size_t n,int64_t deadline){while(n){if(!socket_ready(fd,true,deadline))return false;int k=send(fd,p,n,0);if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;if(k<=0)return false;p+=k;n-=k;}return true;}
static size_t mqtt_string(uint8_t*p,const char*s){size_t n=strlen(s);p[0]=n>>8;p[1]=n;memcpy(p+2,s,n);return n+2;}
static bool probe(const relay_t*t,const char*expected_owner){
 if(!public_target(t->host))return false;
 char *password=NULL,call[8],actual[65];uint32_t uid=0;if(!identity(t,&password,call,&uid,actual)||strcmp(actual,expected_owner)||!password){free(password);return false;}
 char id[64],port[8];snprintf(id,sizeof(id),"M5-check-%lu-%lu",(unsigned long)uid,(unsigned long)t->uid);snprintf(port,sizeof(port),"%d",t->port);
 size_t remaining=10+2+strlen(id)+2+strlen(call)+2+strlen(password);uint8_t*packet=remaining<=6144?malloc(remaining+5):NULL;size_t n=0;
 if(packet){packet[n++]=0x10;size_t v=remaining;do{uint8_t byte=v%128;v/=128;packet[n++]=byte|(v?128:0);}while(v);const uint8_t header[]={0,4,'M','Q','T','T',4,0xc2,0,10};memcpy(packet+n,header,10);n+=10;n+=mqtt_string(packet+n,id);n+=mqtt_string(packet+n,call);n+=mqtt_string(packet+n,password);}
 memset(password,0,strlen(password));free(password);if(!packet)return false;
 struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM},*addr=NULL;bool ok=false;int fd=-1;
 if(!getaddrinfo(t->host,port,&hints,&addr)&&!voice_busy()){fd=socket(AF_INET,SOCK_STREAM,0);if(fd>=0){fcntl(fd,F_SETFL,O_NONBLOCK);int64_t deadline=esp_timer_get_time()+3000000;int c=connect(fd,addr->ai_addr,addr->ai_addrlen);int err=0;socklen_t len=sizeof(err);if((c==0||errno==EINPROGRESS)&&socket_ready(fd,true,deadline)&&!getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len)&&!err&&write_all(fd,packet,n,deadline)){uint8_t ack[4];size_t used=0;while(used<4&&socket_ready(fd,false,deadline)){int k=recv(fd,ack+used,4-used,0);if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;if(k<=0)break;used+=k;}ok=used==4&&ack[0]==0x20&&ack[1]==2&&ack[2]==0&&ack[3]==0;if(ok){const uint8_t disconnect[]={0xe0,0};send(fd,disconnect,2,0);}}}}
 if(fd>=0){close(fd);}if(addr){freeaddrinfo(addr);}memset(packet,0,n);free(packet);return ok;
}
void discovery_tick(void){static int64_t next_fetch=0,next_probe=0,idle_since=0;
 if(!mutex)return;
 char actual[65];if(!current_owner(actual)){atomic_store(&owner_valid,false);idle_since=0;return;}atomic_store(&owner_valid,true);
 xSemaphoreTake(mutex,portMAX_DELAY);if(strcmp(owner,actual)){snprintf(owner,sizeof(owner),"%s",actual);for(int i=0;i<count;i++)rows[i].verified=rows[i].attempted=0;checking_uid=0;revision++;}xSemaphoreGive(mutex);
 int64_t now=esp_timer_get_time();if(voice_busy()){idle_since=0;return;}if(!idle_since)idle_since=now;if(now-idle_since<10000000)return;
 if(now>=next_fetch||atomic_exchange(&refresh,false)){xSemaphoreTake(allocation_lock,portMAX_DELAY);bool fetched=fetch_catalog();xSemaphoreGive(allocation_lock);if(!fetched)status("目录暂不可用，保留已保存中继");next_fetch=now+120000000;}
 if(now<next_probe||voice_busy())return;
 next_probe=now+60000000;
 relay_t target,current;relays_current(&current,NULL);int slot=-1;time_t wall=time(NULL);
 xSemaphoreTake(mutex,portMAX_DELAY);for(int i=0;i<count;i++)if(wall-rows[i].seen<1200&&(!rows[i].attempted||wall-rows[i].attempted>=1800)){slot=i;target=rows[i].relay;rows[i].attempted=wall;checking_uid=target.uid;break;}xSemaphoreGive(mutex);if(slot<0)return;
 char progress[96];snprintf(progress,sizeof(progress),"接入验证 %d/%d",slot+1,count);status(progress);
 bool ok=false;if(same(&target,&current)){ok=relays_verified(&target);}else if(!same_broker(&target,&current)&&!voice_busy())ok=probe(&target,actual);
 char after[65];bool unchanged=current_owner(after)&&!strcmp(actual,after);xSemaphoreTake(mutex,portMAX_DELAY);checking_uid=0;xSemaphoreGive(mutex);if(!unchanged)return;
 xSemaphoreTake(mutex,portMAX_DELAY);if(slot<count&&same(&rows[slot].relay,&target)){rows[slot].verified=ok?wall:0;revision++;snprintf(message,sizeof(message),ok?"本机接入已验证 · %d/%d":"接入验证 · %d/%d",slot+1,count);}xSemaphoreGive(mutex);
}
esp_err_t discovery_start(discovery_identity_fn fn,SemaphoreHandle_t shared){identity=fn;allocation_lock=shared;mutex=xSemaphoreCreateMutex();return mutex?ESP_OK:ESP_ERR_NO_MEM;}
void discovery_status(char*out,size_t size){if(!mutex){snprintf(out,size,"等待联网后自动发现");return;}xSemaphoreTake(mutex,portMAX_DELAY);snprintf(out,size,"%s",message);xSemaphoreGive(mutex);}
void discovery_refresh(void){atomic_store(&refresh,true);}
void discovery_identity_changed(void){
 atomic_store(&owner_valid,false);atomic_store(&refresh,true);if(!mutex)return;
 xSemaphoreTake(mutex,portMAX_DELAY);owner[0]=0;checking_uid=0;
 for(int i=0;i<count;i++)rows[i].verified=rows[i].attempted=0;
 revision++;snprintf(message,sizeof(message),"身份已变更，空闲时刷新并重新验证");xSemaphoreGive(mutex);
}
int discovery_list(relay_t out[DISCOVERY_MAX],uint32_t*rev){if(!mutex)return 0;xSemaphoreTake(mutex,portMAX_DELAY);int n=0;if(atomic_load(&owner_valid))for(int i=0;i<count;i++)if(fresh(&rows[i],time(NULL)))out[n++]=rows[i].relay;if(rev)*rev=revision;xSemaphoreGive(mutex);return n;}
const char*discovery_select(int i,uint32_t rev){relay_t list[DISCOVERY_MAX];uint32_t current;int n=discovery_list(list,&current);if(voice_busy())return "通联中不能切换";char actual[65];if(!current_owner(actual))return "设备身份或网络尚未就绪";xSemaphoreTake(mutex,portMAX_DELAY);bool match=!strcmp(actual,owner);xSemaphoreGive(mutex);if(!match)return "身份已变化，请刷新列表";if(rev!=current||i<0||i>=n)return "列表已更新，请重新选择";
 for(int k=0;k<relays_count();k++){relay_t r;if(!relays_get(k,&r))continue;if(same(&r,&list[i]))return relays_select(k)?NULL:"保存失败";}
 cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name",list[i].name);cJSON_AddStringToObject(j,"callsign",list[i].call);cJSON_AddStringToObject(j,"host",list[i].host);cJSON_AddStringToObject(j,"fingerprint",list[i].fingerprint);cJSON_AddNumberToObject(j,"uid",list[i].uid);cJSON_AddNumberToObject(j,"port",list[i].port);const char*error=relays_add(j);cJSON_Delete(j);if(error)return error;for(int k=0;k<relays_count();k++){relay_t r;if(!relays_get(k,&r))continue;if(same(&r,&list[i]))return relays_select(k)?NULL:"保存失败";}return "保存失败";}
// Save a fresh, signed candidate without switching or asserting login success.
const char*discovery_add(int i,uint32_t rev){
 if(voice_busy())return "通联中不能添加，请稍后重试";
 relay_t target;time_t now=time(NULL);const char*error=NULL;
 xSemaphoreTake(mutex,portMAX_DELAY);
 if(rev!=revision||i<0||i>=count)error="列表已更新，请刷新后重试";
 else if(now<rows[i].seen||now-rows[i].seen>=1200||now<updated||now-updated>=600)error="广播资料已过期，请刷新后重试";
 else target=rows[i].relay;
 xSemaphoreGive(mutex);if(error)return error;
 cJSON*j=cJSON_CreateObject();if(!j)return "内存暂不足，请稍后重试";
 cJSON_AddStringToObject(j,"name",target.name);cJSON_AddStringToObject(j,"callsign",target.call);cJSON_AddStringToObject(j,"host",target.host);cJSON_AddStringToObject(j,"fingerprint",target.fingerprint);cJSON_AddNumberToObject(j,"uid",target.uid);cJSON_AddNumberToObject(j,"port",target.port);
 error=relays_add(j);cJSON_Delete(j);return error;
}
static const char*candidate_status(candidate_t*c,time_t now,bool owned){
 if(now<c->seen||now-c->seen>=1200||now<updated||now-updated>=600)return "stale";
 if(!owned)return "pending";
 if(usable(c,now))return "verified";
 if(checking_uid==c->relay.uid)return "checking";
 return c->attempted?"unconfirmed":"pending";
}
static cJSON*node_json(const relay_t*r){cJSON*v=cJSON_CreateObject();cJSON_AddStringToObject(v,"name",r->name);cJSON_AddStringToObject(v,"callsign",r->call);cJSON_AddNumberToObject(v,"uid",r->uid);return v;}
cJSON*discovery_json(void){
 cJSON*j=cJSON_CreateObject(),*a=cJSON_AddArrayToObject(j,"servers"),*all=cJSON_AddArrayToObject(j,"candidateServers");int indexes[DISCOVERY_MAX];
 xSemaphoreTake(mutex,portMAX_DELAY);time_t now=time(NULL);bool owned=atomic_load(&owner_valid);int verified=0;
 cJSON_AddNumberToObject(j,"revision",revision);cJSON_AddStringToObject(j,"message",message);cJSON_AddNumberToObject(j,"candidates",count);
 for(int i=0;i<count;i++){indexes[i]=-1;if(owned&&fresh(&rows[i],now)){indexes[i]=verified++;cJSON_AddItemToArray(a,node_json(&rows[i].relay));}}
 // Fresh signed candidates are selectable before actual relay authentication.
 for(int i=0;i<count;i++){
  const char*state=candidate_status(&rows[i],now,owned);
  cJSON*v=node_json(&rows[i].relay);cJSON_AddStringToObject(v,"state",state);cJSON_AddNumberToObject(v,"selectIndex",indexes[i]);cJSON_AddNumberToObject(v,"addIndex",strcmp(state,"stale")?i:-1);cJSON_AddItemToArray(all,v);
 }
 xSemaphoreGive(mutex);return j;
}
