#include "cert_wire.h"
#include "monocypher-ed25519.h"
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
static const char b64chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
void wire_b64(const uint8_t *p,size_t n,char *out){size_t k=0;uint32_t acc=0;unsigned bits=0;for(size_t i=0;i<n;i++){acc=(acc<<8)|p[i];bits+=8;while(bits>=6){bits-=6;out[k++]=b64chars[(acc>>bits)&63];}}if(bits)out[k++]=b64chars[(acc<<(6-bits))&63];out[k]=0;}
bool wire_unb64(const char *s,uint8_t *out,size_t n){if(!s||strlen(s)!=(n*8+5)/6)return false;uint32_t a=0;unsigned bits=0;size_t k=0;for(const char *p=s;*p;p++){const char *v=strchr(b64chars,*p);if(!v)return false;a=(a<<6)|(v-b64chars);bits+=6;if(bits>=8){bits-=8;if(k>=n)return false;out[k++]=(a>>bits)&255;}}return k==n&&(!bits||!(a&((1u<<bits)-1)));}
bool wire_callsign(const char *s){if(!s)return false;size_t n=strlen(s);if(n<4||n>6||s[0]!='B'||s[1]<'A'||s[1]>'Z'||s[2]<'0'||s[2]>'9')return false;for(size_t i=3;i<n;i++)if(s[i]<'A'||s[i]>'Z')return false;return true;}
typedef struct {uint8_t *p;size_t n,cap;bool ok;} Buf;
static void byte(Buf*b,uint8_t v){if(b->n>=b->cap){b->ok=false;return;}b->p[b->n++]=v;}
static void head(Buf*b,unsigned m,uint64_t v){if(v<24)byte(b,(m<<5)|v);else{unsigned n=v<=255?1:v<=65535?2:v<=0xffffffff?4:8;byte(b,(m<<5)|(n==1?24:n==2?25:n==4?26:27));for(int i=n-1;i>=0;i--)byte(b,v>>(i*8));}}
static void raw(Buf*b,const uint8_t*p,size_t n){for(size_t i=0;i<n;i++)byte(b,p[i]);}
static void text(Buf*b,const char*s){if(!s){b->ok=false;return;}head(b,3,strlen(s));raw(b,(const uint8_t*)s,strlen(s));}
static void bytes(Buf*b,const uint8_t*p,size_t n){head(b,2,n);raw(b,p,n);}
static const cJSON *get(const cJSON*j,const char*k){return cJSON_GetObjectItemCaseSensitive(j,k);}
static const char *str(const cJSON*j,const char*k){const cJSON*v=get(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool num(const cJSON*j,const char*k,int64_t*out){const cJSON*v=get(j,k);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>9007199254740991.0||floor(v->valuedouble)!=v->valuedouble)return false;*out=(int64_t)v->valuedouble;return true;}
static void jnum(Buf*b,const cJSON*j,const char*k){int64_t n;if(!num(j,k,&n)){b->ok=false;return;}head(b,0,n);}
static void jtext(Buf*b,const cJSON*j,const char*k){text(b,str(j,k));}
static void jpk(Buf*b,const cJSON*j,const char*k){uint8_t pk[32];if(!wire_unb64(str(j,k),pk,32)){b->ok=false;return;}bytes(b,pk,32);}
static bool eq(const char*a,const char*b){return a&&b&&!strcmp(a,b);}
static bool unique_keys(const cJSON*j){if(cJSON_IsObject(j)){for(const cJSON*a=j->child;a;a=a->next){for(const cJSON*b=a->next;b;b=b->next)if(!strcmp(a->string,b->string))return false;if(!unique_keys(a))return false;}}else if(cJSON_IsArray(j)){for(const cJSON*a=j->child;a;a=a->next)if(!unique_keys(a))return false;}return true;}
size_t wire_request_tbs(uint8_t*out,size_t cap,const char*call,const uint8_t pk[32],const char*nonce){Buf b={out,0,cap,true};head(&b,4,6);text(&b,"FMO-M5");head(&b,0,1);text(&b,"certificateRequest");text(&b,call);bytes(&b,pk,32);text(&b,nonce);return b.ok?b.n:0;}
size_t wire_cert_tbs(const cJSON*j,uint8_t*out,size_t cap){const char*kind=str(j,"type");const cJSON*s=get(j,"subject"),*i=get(j,"issuer"),*e=get(j,"extensions");Buf b={out,0,cap,true};unsigned count=eq(kind,"rootCA")?15:eq(kind,"intermediateCA")?20:eq(kind,"userCert")?9:0;if(!count)return 0;head(&b,4,count);text(&b,"FMO");head(&b,0,4);text(&b,kind);
 if(count==9){jnum(&b,j,"issuerSn");jtext(&b,s,"callsign");jnum(&b,s,"uid");jpk(&b,s,"publicKey");}
 else{jnum(&b,j,"sn");if(count==15){jtext(&b,i,"name");jtext(&b,i,"email");jtext(&b,s,"name");jpk(&b,s,"publicKey");}else{jnum(&b,i,"sn");jtext(&b,i,"name");jpk(&b,i,"publicKey");jtext(&b,s,"name");jtext(&b,s,"email");jpk(&b,s,"publicKey");}if(!cJSON_IsTrue(get(e,"isCA")))b.ok=false;byte(&b,0xf5);jnum(&b,e,"pathLen");if(count==20)jtext(&b,e,"keyId");jtext(&b,e,"crl");jtext(&b,e,"license");if(count==15)jtext(&b,e,"keyId");else{const cJSON*r=get(e,"uidRange");jnum(&b,r,"start");jnum(&b,r,"end");const cJSON*c=get(e,"issuingCountries");int n=cJSON_GetArraySize(c);if(!cJSON_IsArray(c)||n<1||n>32)b.ok=false;const char*cs[32];for(int x=0;x<n&&x<32;x++){const cJSON*v=cJSON_GetArrayItem(c,x);cs[x]=cJSON_IsString(v)?v->valuestring:"";if(strlen(cs[x])!=2||cs[x][0]<'A'||cs[x][0]>'Z'||cs[x][1]<'A'||cs[x][1]>'Z')b.ok=false;}if(n>32)return 0;for(int x=0;x<n;x++)for(int y=x+1;y<n;y++)if(strcmp(cs[x],cs[y])>0){const char*t=cs[x];cs[x]=cs[y];cs[y]=t;}head(&b,4,n);for(int x=0;x<n;x++)text(&b,cs[x]);}}
 jnum(&b,j,"iat");jnum(&b,j,"exp");return b.ok?b.n:0;}
static bool verify(const cJSON*cert,const uint8_t pk[32],uint8_t*scratch){uint8_t sig[64];if(!eq(str(cert,"signatureAlgorithm"),"Ed25519")||!wire_unb64(str(cert,"signature"),sig,64))return false;size_t n=wire_cert_tbs(cert,scratch,4096);return n&&crypto_ed25519_check(sig,pk,scratch,n)==0;}
const char *wire_validate_root(const cJSON*r,int64_t now){
 if(!cJSON_IsObject(r)||!unique_keys(r)||!eq(str(r,"type"),"rootCA"))return "请选择 FMO JSON 根 CA 证书";
 const cJSON*s=get(r,"subject"),*e=get(r,"extensions");uint8_t pub[32];int64_t sn,iat,exp,path;
 const char*name=str(s,"name");if(!name||!name[0]||strlen(name)>96||!eq(name,str(get(r,"issuer"),"name"))||!wire_unb64(str(s,"publicKey"),pub,32))return "根证书名称或公钥无效";
 if(!num(r,"sn",&sn)||!num(r,"iat",&iat)||!num(r,"exp",&exp)||!num(e,"pathLen",&path)||path!=1||!cJSON_IsTrue(get(e,"isCA"))||iat>=exp)return "根证书授权或有效期无效";
 if(now<1700000000)return "请先联网校时，再导入根证书";
 if(now<iat||now>=exp)return "根证书未生效或已过期";
 uint8_t*scratch=malloc(4096);if(!scratch)return "可用内存不足";bool ok=verify(r,pub,scratch);free(scratch);return ok?NULL:"根证书自签名校验失败";
}
const char *wire_validate_bundle(const cJSON*b,const uint8_t own[32],const char*call,int64_t now){
 if(!cJSON_IsObject(b)||!unique_keys(b))return "JSON 格式不正确或包含重复字段";
 const cJSON*r=get(b,"rootCert"),*i=get(b,"intermediateCert"),*u=get(b,"userCert");if(!eq(str(r,"type"),"rootCA")||!eq(str(i,"type"),"intermediateCA")||!eq(str(u,"type"),"userCert"))return "需要 rootCert、intermediateCert、userCert 完整证书链";
 const cJSON*rs=get(r,"subject"),*is=get(i,"subject"),*us=get(u,"subject"),*ri=get(r,"issuer"),*ii=get(i,"issuer"),*re=get(r,"extensions"),*ie=get(i,"extensions");
 uint8_t rp[32],ip[32],up[32],issuer_pk[32];if(!wire_unb64(str(rs,"publicKey"),rp,32)||!wire_unb64(str(is,"publicKey"),ip,32)||!wire_unb64(str(us,"publicKey"),up,32)||!wire_unb64(str(ii,"publicKey"),issuer_pk,32))return "公钥格式错误";
 if(memcmp(up,own,32))return "证书不属于这台 M5";
 if(!wire_callsign(str(us,"callsign"))||!eq(call,str(us,"callsign")))return "证书呼号与设备申请不一致";
 int64_t rsn,isn,irsn,uisn,rpath,ipath,start,end,uid,rt,it,ut,rx,ix,ux;
 if(!num(r,"sn",&rsn)||!num(i,"sn",&isn)||!num(ii,"sn",&irsn)||!num(u,"issuerSn",&uisn)||!num(re,"pathLen",&rpath)||!num(ie,"pathLen",&ipath)||!num(get(ie,"uidRange"),"start",&start)||!num(get(ie,"uidRange"),"end",&end)||!num(us,"uid",&uid)||!num(r,"iat",&rt)||!num(r,"exp",&rx)||!num(i,"iat",&it)||!num(i,"exp",&ix)||!num(u,"iat",&ut)||!num(u,"exp",&ux))return "证书数值字段无效";
 if(rsn!=irsn||isn!=uisn||rpath!=1||ipath!=0||!eq(str(ri,"name"),str(rs,"name"))||!eq(str(ii,"name"),str(rs,"name"))||memcmp(rp,issuer_pk,32))return "证书签发关系不匹配";
 if(uid<1||uid>0xffffffffLL||uid==1791||start>end||uid<start||uid>end)return "设备 UID 超出授权范围或与测试台冲突";
 if(rt>=rx||it>=ix||ut>=ux||it<rt||ix>rx||ut<it||ux>ix)return "证书有效期层级不正确";
 if(now>1700000000&&(now<rt||now<it||now<ut||now>=rx||now>=ix||now>=ux))return "证书未生效或已经过期";
 bool cn=false;const cJSON*c; cJSON_ArrayForEach(c,get(ie,"issuingCountries")){if(cJSON_IsString(c)&&!strcmp(c->valuestring,"CN"))cn=true;}if(!cn)return "签发机构未获授权签发中国呼号";
 uint8_t *buf=malloc(4096);if(!buf)return "可用内存不足";bool ok=verify(r,rp,buf)&&verify(i,rp,buf)&&verify(u,ip,buf);free(buf);if(!ok)return "证书数字签名验证失败";return NULL;
}

size_t wire_login_tbs(uint8_t*out,size_t cap,int64_t uid,const char*call,const char*host,int port,const uint8_t sfp[32],int64_t now,const uint8_t ufp[32]){Buf b={out,0,cap,true};head(&b,4,12);text(&b,"FMO");head(&b,0,4);text(&b,"serverAuthorizerReqHttp");head(&b,0,uid);text(&b,call);head(&b,0,uid);text(&b,"user");text(&b,host);head(&b,0,port);bytes(&b,sfp,32);head(&b,0,now);bytes(&b,ufp,32);return b.ok?b.n:0;}
