#include "station_beacon.h"
#include "cert_wire.h"
#include "monocypher-ed25519.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
size_t station_beacon(char out[513],const station_settings_t*s,const cJSON*u,const uint8_t secret[64],int64_t now,beacon_hash_fn hash){
 if(!station_valid(s)||!s->has_location||s->frequency<=0||now<1700000000)return 0;
 const cJSON*subject=cJSON_GetObjectItemCaseSensitive(u,"subject"),*call=cJSON_GetObjectItemCaseSensitive(subject,"callsign");if(!cJSON_IsString(call)||!wire_callsign(call->valuestring))return 0;
 long lat=lround(fabs(s->latitude)*6000),lon=lround(fabs(s->longitude)*6000);char la[64],lo[64],freq[20];
 snprintf(la,sizeof(la),"%02ld%02ld.%02ld%c",lat/6000,(lat/100)%60,lat%100,signbit(s->latitude)?'S':'N');snprintf(lo,sizeof(lo),"%03ld%02ld.%02ld%c",lon/6000,(lon/100)%60,lon%100,signbit(s->longitude)?'W':'E');snprintf(freq,sizeof(freq),"%.4f",s->frequency);
 uint8_t blob[256],digest[32],tbs[384],sig[64];char cert[344],signature[88],sender[12];size_t n=wire_user_blob(u,blob,sizeof(blob));if(!n||!hash(blob,n,digest))return 0;wire_b64(blob,n,cert);
 n=wire_beacon_tbs(tbs,sizeof(tbs),call->valuestring,s->aprs_ssid,la,lo,digest,freq,(unsigned)s->height,s->rig,s->antenna,now/600);if(!n)return 0;crypto_ed25519_sign(sig,secret,tbs,n);wire_b64(sig,64,signature);
 snprintf(sender,sizeof(sender),"%s",call->valuestring);if(s->aprs_ssid)snprintf(sender,sizeof(sender),"%s-%u",call->valuestring,s->aprs_ssid);
 int used=snprintf(out,513,"%s>APFMO4,TCPIP*:=%s/%sI FMO-V4,BEACON,CERT:%s,FREQ:%s",sender,la,lo,cert,freq);if(used<0||used>=513)return 0;
 #define ADD(...) do{int k=snprintf(out+used,513-used,__VA_ARGS__);if(k<0||k>=513-used)return 0;used+=k;}while(0)
 if(s->height>0)ADD(",HEIGHT:%u",(unsigned)s->height);
 if(s->rig[0])ADD(",RIG:%s",s->rig);
 if(s->antenna[0])ADD(",ANT:%s",s->antenna);
 ADD(",SIG:%s\r\n",signature);
 #undef ADD
 return used<=512?(size_t)used:0;
}
