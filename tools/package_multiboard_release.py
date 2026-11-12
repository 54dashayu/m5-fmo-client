#!/usr/bin/env python3
"""Stage public multi-board binaries; never accesses hardware, identities or networks."""
from pathlib import Path
import argparse,json,hashlib,subprocess,zipfile,struct,shutil
p=argparse.ArgumentParser();p.add_argument('--preview',action='store_true');args=p.parse_args()
r=Path(__file__).resolve().parents[1]
if not args.preview:assert not subprocess.check_output(['git','status','--porcelain'],cwd=r,text=True).strip(),'Commit release source first'
sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip()
for board,folder,chip,chipid,capacity in [('m5-fire','build-m5-regression','ESP32',0,'4MB'),('xingzhi-cube-1.54tft-wifi','build-xingzhi','ESP32-S3',9,'16MB')]:
 build=r/folder;desc=json.loads((build/'project_description.json').read_text());version=desc['project_version'];assert version=='0.7.30'
 flash=json.loads((build/'flasher_args.json').read_text());assert flash['extra_esptool_args']['chip']==('esp32' if chipid==0 else 'esp32s3')
 base=board+'/v'+version;out=r/'web-flasher/public/firmware'/base;out.mkdir(parents=True,exist_ok=True)
 parts=[]
 sources=[(int(flash['bootloader']['offset'],16),'bootloader.bin',build/flash['bootloader']['file']),(0x8000,'partition-table.bin',build/flash['partition-table']['file'])]
 if chipid==9:sources.append((0x9000,'initial-settings.bin',None))
 sources.append((int(flash['app']['offset'],16),'application.bin',build/flash['app']['file']))
 for addr,name,source in sources:
  data=source.read_bytes() if source else b'\xff'*0x18000
  if name in ['bootloader.bin','application.bin']:
   assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==chipid
  if name=='application.bin':assert data[48:80].split(b'\0')[0].decode()==version
  (out/name).write_bytes(data);parts.append(dict(path=base+'/'+name,address=addr,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)))
 zipname=f'fmo-{board}-v{version}.zip';manifest=dict(version=version,board=board,chip=chip,flashSize=capacity,sourceCommit=sha,parts=parts,download=base+'/'+zipname)
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(out.parent/'manifest.json').write_bytes((out/'manifest.json').read_bytes())
 (out/'README.md').write_bytes((r/'docs/RELEASE_0.7.30.md').read_bytes())
 checks=''.join(f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n' for f in sorted(out.iterdir()) if f.is_file() and f.name!='SHA256SUMS' and f.suffix!='.zip')
 (out/'SHA256SUMS').write_text(checks)
 with zipfile.ZipFile(out/zipname,'w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(out.iterdir()):
   if f.is_file() and f.suffix!='.zip':z.write(f,f.name)
 with (out/'SHA256SUMS').open('a') as f:f.write(hashlib.sha256((out/zipname).read_bytes()).hexdigest()+'  '+zipname+'\n')
 dist=r/'artifacts/releases'/('v'+version)/board;dist.mkdir(parents=True,exist_ok=True)
 for f in out.iterdir():
  if f.is_file():shutil.copy2(f,dist/f.name)
 print(board,zipname,'SHA256',hashlib.sha256((out/zipname).read_bytes()).hexdigest())
