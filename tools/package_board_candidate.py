#!/usr/bin/env python3
"""Package a board-specific local candidate; never flashes or publishes."""
import argparse,hashlib,json,struct,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('board',choices=['m5','xingzhi']);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
board,chip,chip_id,build=('xingzhi-cube-1.54tft-wifi','esp32s3',9,r/'build-xingzhi') if a.board=='xingzhi' else ('m5-fire','esp32',0,r/'build-m5-regression')
desc=json.loads((build/'project_description.json').read_text());version=desc['project_version']
assert desc['target']==chip
assert not subprocess.check_output(['git','status','--porcelain'],cwd=r,text=True).strip(),'Commit source before packaging'
flash=json.loads((build/'flasher_args.json').read_text());assert flash['extra_esptool_args']['chip']==chip
out=r/'artifacts'/board/version;out.mkdir(parents=True,exist_ok=True);parts=[]
for address,source in flash['flash_files'].items():
 data=(build/source).read_bytes();name=Path(source).name
 if source==flash['app']['file']:
  assert data[48:80].split(b'\0',1)[0].decode()==version
  name=f'fmo-{board}-{version}.bin'
 if source!=flash['partition-table']['file']:assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==chip_id
 (out/name).write_bytes(data);parts.append(dict(path=name,address=int(address,16),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
manifest=dict(board=board,chip=chip,version=version,status='built-not-device-tested',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),flashSettings=flash['flash_settings'],parts=parts)
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'README.md').write_bytes((r/'docs/MULTIBOARD_0.7.30_DEV1.md').read_bytes())
(out/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n' for f in sorted(out.iterdir()) if f.is_file() and f.name!='SHA256SUMS' and f.suffix!='.zip'))
archive=out/f'fmo-{board}-{version}-candidate.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(out.iterdir()):
  if f.is_file() and f.suffix!='.zip':z.write(f,f.name)
print(archive);print('SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())
