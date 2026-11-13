#!/usr/bin/env python3
"""Local-only USB transport for the M5 management page. No credential logs/files."""
import argparse,json,secrets,threading,time,termios,re
from collections import deque
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from pathlib import Path
import serial
from serial.tools import list_ports

class Link:
 def __init__(self,port):self.port=port;self.serial=None;self.lock=threading.Lock();self.events=deque(maxlen=80)
 def diagnostic(self,line):
  diagnostic=re.search(r'(rst:0x[0-9a-fA-F]+[^\r\n]*|Brownout detector was triggered|Guru Meditation Error[^\r\n]*|A stack overflow in task [A-Za-z0-9_ -]+ has been detected|Stack canary watchpoint triggered[^\r\n]*|Backtrace: [0-9a-fx: ]+|buttons: key=-?\d+ action=\d+|voice: stop=[a-z_]+|mqtt_client: Writing failed: errno=\d+|mqtt_client: Writing didn\'t complete in specified timeout: errno=\d+|mqtt_client: Poll read error: \d+, aborting connection|mqtt_client: mqtt_process_receive: mqtt_message_receive\(\) returned -?\d+|mqtt_client: esp_mqtt_handle_transport_read_error: transport_read\(\) error: errno=\d+|mqtt_client: No PING_RESP, disconnected|mqtt_client: [A-Za-z_]+: Network timeout while reading MQTT message|mqtt_client: MQTT connect failed|device_web: Wi-Fi disconnected: reason=\d+ RSSI=-?\d+|device_web: MQTT recycle wifi=\d+ valid=\d+ changed=\d+ phase=\d+|device_web: MQTT error type=\d+ socket=\d+ tls=\d+ broker=\d+)',line.decode(errors='replace'))
  if diagnostic:self.events.append({'time':round(time.time()),'event':diagnostic.group(1)})
 def call(self,op,data=None):
  with self.lock:
   try:
    if self.serial is None:
     s=serial.Serial();s.port=self.port;s.baudrate=115200;s.timeout=.15;s.write_timeout=2;s.dtr=False;s.rts=False;s.open();self.serial=s;self.events.append({"time":round(time.time()),"event":"host_serial_open"})
     time.sleep(5)
    s=self.serial
    pending=getattr(s,'in_waiting',0)
    if pending:
     for line in s.read(min(pending,16384)).splitlines():self.diagnostic(line)
    s.reset_input_buffer();rid=secrets.token_hex(8)
    packet=json.dumps(dict(id=rid,op=op,data=data or {}),ensure_ascii=True,separators=(',',':')).encode()
    if len(packet)>8192:raise ValueError('请求过大')
    s.write(b'@M5:'+packet+b'\n');s.flush();deadline=time.monotonic()+12
    while time.monotonic()<deadline:
     line=s.readline(16384);at=line.find(b'@M5:')
     if at<0:
      self.diagnostic(line)
      continue
     try:reply=json.loads(line[at+4:])
     except (ValueError,UnicodeError):continue
     if reply.get('id')==rid:
      reply.pop('id',None);return reply
    raise TimeoutError('M5 未回应。请确认设备已开机；若刚重新连接，稍后刷新。')
   except TimeoutError:
    # A slow firmware response is not a physical disconnect. Keep DTR/RTS stable.
    raise
   except (serial.SerialException,OSError,termios.error) as error:
    self.events.append({"time":round(time.time()),"event":"host_serial_error:"+type(error).__name__})
    if self.serial:
     try:self.serial.close()
     except OSError:pass
    self.serial=None
    raise

def render_management_page(template, s3=False):
 values={'FMO_WEB_TITLE':'ESP32/小智设备管理' if s3 else '机婶婶 · M5 设备管理','FMO_WEB_DEVICE':'小智' if s3 else 'M5','FMO_WEB_MODEL':'ESP32-S3 · 小智' if s3 else 'M5 FIRE','FMO_WEB_PREFIX':'E.' if s3 else 'M.'}
 for key,value in values.items():template=template.replace('@'+key+'@',value)
 return template.encode()

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--serial');parser.add_argument('--port',type=int,default=8772);args=parser.parse_args()
 if not args.serial:
  ports=[p.device for p in list_ports.comports() if p.vid==0x10C4 and p.pid==0xEA60]
  if len(ports)!=1:parser.error('请使用 --serial 指定 M5 串口；当前 CP2104 数量：'+str(len(ports)))
  args.serial=ports[0]
 link=Link(args.serial);token=secrets.token_urlsafe(32);authority=f'127.0.0.1:{args.port}';origin='http://'+authority
 device=next((p for p in list_ports.comports() if p.device==args.serial),None)
 page=render_management_page((Path(__file__).resolve().parents[1]/'ui-preview-firmware/main/web.html').read_text(),bool(device and device.vid==0x303a and device.pid==0x1001))
 class Handler(BaseHTTPRequestHandler):
  def log_message(self,*args):pass
  def send(self,status,value,html=False):
   raw=value if html else json.dumps(value,ensure_ascii=False).encode();self.send_response(status)
   self.send_header('Content-Type','text/html; charset=utf-8' if html else 'application/json; charset=utf-8');self.send_header('Content-Length',str(len(raw)));self.send_header('Cache-Control','no-store');self.send_header('X-Frame-Options','DENY');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self' https://fmo.bh1jss.net https://fmo.simy.live; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'");self.end_headers()
   try:self.wfile.write(raw)
   except (BrokenPipeError,ConnectionResetError):pass
  def run_api(self):
   if self.headers.get('Host')!=authority or self.headers.get('Origin',origin)!=origin or self.headers.get('Sec-Fetch-Site','same-origin') not in ('same-origin','none'):
    return self.send(403,{'error':'仅允许本机同源访问'})
   if self.command=='GET' and self.path=='/':return self.send(200,page,True)
   if self.command=='GET' and self.path=='/favicon.ico':return self.send(204,{})
   if self.command=='GET' and self.path=='/api/transport':return self.send(200,{'usb':True,'token':token})
   if self.headers.get('X-Device-Token')!=token:return self.send(401,{'error':'请刷新本机管理页面'})
   if self.command=='GET' and self.path=='/api/diagnostics':return self.send(200,{'events':list(link.events)})
   routes={('POST','/api/roots'):'roots',('POST','/api/relays/page'):'relays_paged',('POST','/api/discovery/add'):'discovery_add',('GET','/api/discovery'):'discovery',('POST','/api/discovery/refresh'):'discovery_refresh',('POST','/api/discovery/select'):'discovery_select',('POST','/api/logout'):'logout',('POST','/api/relay/name'):'relay_name',('POST','/api/relay/delete'):'relay_delete',('POST','/api/identity/reset'):'identity_reset',('POST','/api/certificate/rollback'):'rollback',('POST','/api/scan'):'scan',('GET','/api/networks'):'networks',('POST','/api/network/connect'):'network_connect',('POST','/api/network/delete'):'network_delete',('GET','/api/status'):'status',('GET','/api/relays'):'relays',('POST','/api/relays'):'relay_add',('POST','/api/relay/select'):'relay_select',('POST','/api/wifi'):'wifi',('POST','/api/reconnect'):'reconnect',('POST','/api/request'):'request',('POST','/api/certificate'):'certificate',('DELETE','/api/certificate'):'delete'}
   op=routes.get((self.command,self.path))
   if not op:return self.send(404,{'error':'未知接口'})
   data={}
   try:
    n=int(self.headers.get('Content-Length','0'))
    if n<0 or n>8192:return self.send(413,{'error':'请求过大'})
    if n:data=json.loads(self.rfile.read(n))
    if not isinstance(data,dict):raise ValueError()
    reply=link.call(op,data);return self.send(400 if reply.get('ok') is False else 200,reply)
   except (ValueError,RecursionError):return self.send(400,{'error':'JSON 请求无效'})
   except (serial.SerialException,OSError,termios.error,TimeoutError):return self.send(503,{'error':'USB 暂时未连接或设备未回应，请检查连接后重试。'})
  do_GET=do_POST=do_DELETE=run_api
  def setup(self):super().setup();self.connection.settimeout(15)
 server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler);server.daemon_threads=True
 print(f'M5 USB 管理已启动：{origin}（仅本机可访问，不记录密码）',flush=True)
 try:server.serve_forever()
 except KeyboardInterrupt:pass
 finally:
  server.server_close()
  if link.serial:link.serial.close()
if __name__=='__main__':main()
