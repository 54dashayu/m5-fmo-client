#!/usr/bin/env python3
"""Package the two aligned development builds into the isolated SSTV flasher channel."""
from pathlib import Path
import json,hashlib,struct,subprocess,zipfile
r=Path(__file__).resolve().parents[1];public=r/'web-flasher/public';out=public/'sstv-preview'
sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip()
assert not subprocess.check_output(['git','status','--porcelain'],cwd=r,text=True).strip(),'Commit source first'
versions=[]
for board,folder,chip,chipid in [('m5-fire','build-m5-regression','ESP32',0),('xingzhi-cube-1.54tft-wifi','build-xingzhi','ESP32-S3',9)]:
 build=r/folder;version=json.loads((build/'project_description.json').read_text())['project_version'];assert version=='0.7.75-dev.19';versions.append(version)
 flash=json.loads((build/'flasher_args.json').read_text());base=board+'/v'+version;dest=out/'firmware'/base;dest.mkdir(parents=True,exist_ok=True);parts=[]
 sources=[(int(flash['bootloader']['offset'],16),'bootloader.bin',build/flash['bootloader']['file']),(0x8000,'partition-table.bin',build/flash['partition-table']['file'])]
 if chipid==9:sources.append((0x9000,'initial-settings.bin',None))
 sources.append((int(flash['app']['offset'],16),'application.bin',build/flash['app']['file']))
 for address,name,path in sources:
  data=path.read_bytes() if path else b'\xff'*0x18000
  if name in ('application.bin','bootloader.bin'):assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==chipid
  if name=='application.bin':assert data[48:80].split(b'\0')[0].decode()==version
  (dest/name).write_bytes(data);parts.append(dict(path=base+'/'+name,address=address,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)))
 zipname='fmo-'+board+'-v'+version+'.zip'
 manifest=dict(version=version,board=board,chip=chip,flashSize='16MB',sourceCommit=sha,sourceDirty=False,parts=parts,download=base+'/'+zipname)
 (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(dest.parent/'manifest.json').write_bytes((dest/'manifest.json').read_bytes());(dest/'README.txt').write_bytes((out/'release.txt').read_bytes())
 files=sorted(dest.glob('*'));(dest/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in files if p.is_file() and p.name!='SHA256SUMS' and p.suffix!='.zip'))
 with zipfile.ZipFile(dest/zipname,'w',zipfile.ZIP_DEFLATED) as z:
  for p in sorted(dest.iterdir()):
   if p.is_file() and p.suffix!='.zip':z.write(p,p.name)
 with (dest/'SHA256SUMS').open('a') as f:f.write(hashlib.sha256((dest/zipname).read_bytes()).hexdigest()+'  '+zipname+'\n')
assert len(set(versions))==1
(out/'app.js').write_text((public/'app.js').read_text().replace('photos/','../photos/').replace('20261114.1','20261116.sstv19'))
print('Packaged aligned SSTV preview:',versions[0],sha)
