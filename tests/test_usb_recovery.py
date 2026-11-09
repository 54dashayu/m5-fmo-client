"""A removed CP2104 raises termios.error; the next request must reopen it."""
import json,sys,termios
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from usb_admin import Link
class Port:
 def __init__(self,broken=False):self.broken=broken;self.closed=False;self.request=None
 def open(self):pass
 def reset_input_buffer(self):
  if self.broken:raise termios.error(6,'Device not configured')
 def write(self,data):self.request=json.loads(data[4:])
 def flush(self):pass
 def readline(self,*args):return b'@M5:'+json.dumps({'id':self.request['id'],'version':'test'}).encode()+b'\n'
 def close(self):self.closed=True
old,new=Port(True),Port()
with patch('usb_admin.serial.Serial',side_effect=[old,new]),patch('usb_admin.time.sleep'):
 link=Link('test-port')
 try:link.call('status');raise AssertionError('Expected disconnect')
 except termios.error:pass
 assert old.closed and link.serial is None
 assert link.call('status')=={'version':'test'}
print('PASS removed serial handle is released; next request reconnects')

port=Port()
link=Link('test-port');link.serial=port
with patch('usb_admin.time.monotonic',side_effect=[0,13]):
 try:link.call('status');raise AssertionError('Expected timeout')
 except TimeoutError:pass
assert link.serial is port and not port.closed
assert link.call('status')=={'version':'test'}
print('PASS response timeout preserves serial handle and next request succeeds without reopening')

link=Link('test-port')
link.diagnostic(b'***ERROR*** A stack overflow in task voice_audio has been detected.')
assert link.events[-1]['event']=='A stack overflow in task voice_audio has been detected'
print('PASS crash diagnostics preserve the overflowing task name')

# Keep transport failures useful without retaining MQTT credentials or payloads.
link = Link('/dev/test-unused')
for event in [
    'mqtt_client: Writing failed: errno=104',
    "mqtt_client: Writing didn't complete in specified timeout: errno=11",
    'mqtt_client: Poll read error: 104, aborting connection',
    'mqtt_client: mqtt_process_receive: mqtt_message_receive() returned -2',
]:
    link.diagnostic(('E (123) '+event).encode())
    assert link.events[-1]['event'] == event
count=len(link.events)
link.diagnostic(b'mqtt_client: username=secret password=do-not-store payload=private')
assert len(link.events)==count
print('PASS bounded transport error whitelist excludes credentials and payloads')
