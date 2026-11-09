#!/usr/bin/env python3
"""Package public firmware only, never device NVS or credentials."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,zipfile
parser=argparse.ArgumentParser()
parser.add_argument("--app-bin",type=Path,help="Use a previously device-validated application image")
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
version=re.search(r'PROJECT_VER "([^"]+)"',(root/'ui-preview-firmware/CMakeLists.txt').read_text())[1]
build=root/'ui-preview-firmware/build'
out=root/'release-dist'/('v'+version);out.mkdir(parents=True,exist_ok=True)
parts=[]
for source,name,offset in [('bootloader/bootloader.bin','bootloader.bin',0x1000),('partition_table/partition-table.bin','partition-table.bin',0x8000),('m5-fmo-ui-preview.bin',f'm5-fmo-client-v{version}-app.bin',0x10000)]:
 data=(args.app_bin if offset==0x10000 and args.app_bin else build/source).read_bytes()
 if offset==0x10000:
  assert data[48:80].split(b"\0",1)[0].decode()==version, "application version does not match source"
 assert offset+len(data)<={0x1000:0x8000,0x8000:0x9000,0x10000:0x310000}[offset], 'image overlaps protected data'
 (out/name).write_bytes(data);parts.append(dict(path=name,address=offset,sha256=hashlib.sha256(data).hexdigest()))
(out/'manifest.json').write_text(json.dumps(dict(version=version,chip='ESP32',parts=parts),indent=2)+'\n')
shutil.copy(root/'docs/BUILD.md',out/'刷写说明.md')
(out/'SHA256SUMS').write_text(''.join(f"{p['sha256']}  {p['path']}\n" for p in parts))
zipname=f'm5-fmo-client-v{version}.zip'
with zipfile.ZipFile(out/zipname,'w',zipfile.ZIP_DEFLATED) as z:
 for f in [*(p['path'] for p in parts),'manifest.json','SHA256SUMS','刷写说明.md']:z.write(out/f,f)
with (out/'SHA256SUMS').open('a') as f:f.write(hashlib.sha256((out/zipname).read_bytes()).hexdigest()+'  '+zipname+'\n')
web=root/'web-flasher/public/firmware';web.mkdir(parents=True,exist_ok=True)
for name in [*(p['path'] for p in parts),'manifest.json','SHA256SUMS',zipname]:shutil.copy(out/name,web/name)
print(out)
