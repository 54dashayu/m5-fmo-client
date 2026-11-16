"""Check browser payload values against the existing evidence-service contract."""
from pathlib import Path
from html.parser import HTMLParser
import re,sys
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from usb_admin import render_management_page
class DeviceKind(HTMLParser):
 def __init__(self):super().__init__();self.active=False;self.values=[];self.labels=[]
 def handle_starttag(self,tag,attrs):
  attrs=dict(attrs)
  if tag=='select':self.active=attrs.get('id')=='deviceKind'
  if tag=='option' and self.active:self.values.append(attrs.get('value'))
 def handle_endtag(self,tag):
  if tag=='select':self.active=False
 def handle_data(self,data):
  if self.active:self.labels.append(data)
s=(root/'ui-preview-firmware/main/web.html').read_text()
cmake=(root/'ui-preview-firmware/main/CMakeLists.txt').read_text()
for s3,expected,label in [(True,'其他 ESP32','小智'),(False,'M5','M5')]:
 page=render_management_page(s,s3).decode();parser=DeviceKind();parser.feed(page)
 assert parser.values==[expected] and label in parser.labels
 branch=cmake.split('elseif(IDF_TARGET STREQUAL "esp32")')[0 if s3 else 1]
 assert re.search(r'set\(FMO_WEB_KIND "'+expected+r'"\)',branch)
 assert expected in ['M5','AI Passport','其他 ESP32']
 assert '@FMO_WEB_KIND@' not in page
assert "upload.status===429" in s
print('PASS M5/Xiaozhi displayed names map to accepted enrollment values in USB and firmware pages')
