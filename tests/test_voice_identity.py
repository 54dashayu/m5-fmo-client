"""Exercise the actual identity setter through real wire packing, without RF/network."""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];main=root/'ui-preview-firmware/main'
src=(main/'voice.c').read_text();fn=src[src.index('void voice_identity('):src.index('void voice_connected(')]
pre='''#include "voice_wire.h"
#include "board_identity.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
static uint32_t uid,server_uid;static char own_call[13];
'''
tail='''
int main(void){
#ifdef CONFIG_FMO_BOARD_XINGZHI
 assert(!strcmp(FMO_CALL_PREFIX,"E."));
#else
 assert(!strcmp(FMO_CALL_PREFIX,"M."));
#endif
 const char*users[]={"BH1JSS","BH1OFJ","B1AA"};
 for(unsigned k=0;k<3;k++){
  char certificate_call[7];strcpy(certificate_call,users[k]);
  voice_identity(17910000+k,certificate_call,1791);
  assert(!strcmp(certificate_call,users[k]));assert(uid==17910000+k&&server_uid==1791);
  char expected[13];snprintf(expected,sizeof(expected),FMO_CALL_PREFIX "%s",users[k]);assert(!strcmp(own_call,expected));
  int16_t pcm[640]={0};uint8_t frames[328],packet[1400];int predictor=0,index=0;
  radpcm_encode(pcm,frames,0,&predictor,&index);
  // Speech and Roger tail share this packet path.
  for(int i=1;i<=5;i++){
   size_t n=voice_pack(packet,sizeof(packet),frames,1,uid,own_call,server_uid,1000,1000+(i-1)*80,i);
   assert(n==408&&!strcmp((char*)packet+10,expected));
   voice_packet_t parsed;assert(voice_parse(packet,n,&parsed));assert(!strcmp(parsed.call,expected)&&parsed.uid==uid&&parsed.server==1791);
  }
 }
 puts("PASS actual board identity setter and speech/tail wire callsign prefix; certificate input unchanged");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+fn+tail)
 for flags in [[],['-DCONFIG_FMO_BOARD_XINGZHI=1']]:
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(main),'-I'+str(root/'ui-preview-firmware/components/fmo_board'),*flags,str(p),str(main/'voice_wire.c'),'-o',td+'/test'],check=True)
  subprocess.run([td+'/test'],check=True)
