"""Actual transport against localhost APRS peers; no public network or real identities."""
from pathlib import Path
import ctypes as C, subprocess, tempfile, socket, threading
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/station_tx.c').read_text()
code=s[s.index('static bool ready('):s.index('static void worker(')]
pre=r'''
#define FMO_VERSION "test-version"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef struct {char line[513],login[12],host[64];int port,passcode;}station_tx_packet_t;
static int64_t esp_timer_get_time(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000LL+t.tv_nsec/1000;}
static bool live(unsigned g){return g==1;}
static void status(const char*s){(void)s;}
'''
post=r'''
int probe(int port,unsigned g){station_tx_packet_t p={.line="TEST>APFMO4:test\r\n",.login="TEST-1",.host="127.0.0.1",.passcode=12345};p.port=port;return transmit(&p,g);}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'t.c').write_text(pre+code+post);subprocess.run(['cc','-shared','-fPIC','-O2',str(p/'t.c'),'-o',str(p/'t.dylib')],check=True)
 lib=C.CDLL(str(p/'t.dylib'));lib.probe.argtypes=[C.c_int,C.c_uint];lib.probe.restype=C.c_int
 for state,expected in [('TEST-1 verified',True),('TEST-1 unverified',False),('OTHER verified',False)]:
  server=socket.socket();server.bind(('127.0.0.1',0));server.listen();port=server.getsockname()[1];received=[]
  def peer():
   conn,_=server.accept();conn.settimeout(3)
   with conn:
    f=conn.makefile('rb');conn.sendall(b'# local test\r\n');received.append(f.readline());response=('# logresp '+state+', server local\r\n').encode()
    # Deliberately fragment protocol response across socket reads.
    for c in response:conn.sendall(bytes([c]))
    received.append(f.readline())
  t=threading.Thread(target=peer);t.start();assert bool(lib.probe(port,1))==expected;t.join(4);server.close();assert not t.is_alive()
  assert received[0]==b'user TEST-1 pass 12345 vers FMO-ESP32 test-version\r\n'
  assert received[1]==(b'TEST>APFMO4:test\r\n' if expected else b'')
 assert not lib.probe(1,2)
 print('PASS actual APRS transport: fragmented banner/login reply, verified login only, wrong identity/unverified rejected, CRLF payload, cancellation before connection')
