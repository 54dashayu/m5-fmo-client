#!/usr/bin/env python3
"""Package an unverified S3 candidate locally; never updates public flasher assets."""
import hashlib,json,struct,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];build=root/'build-xingzhi'
desc=json.loads((build/'project_description.json').read_text());version=desc['project_version']
assert desc['target']=='esp32s3' and version=='0.7.29-s3.1'
cfg=(build/'config/sdkconfig.json').read_text();config=json.loads(cfg)
assert config.get('FMO_BOARD_XINGZHI') and config.get('ESPTOOLPY_FLASHSIZE')=='16MB'
board='xingzhi-cube-1.54tft-wifi';out=root/'artifacts'/board/version;out.mkdir(parents=True,exist_ok=True)
parts=[]
for src,name,address in [('bootloader/bootloader.bin','bootloader.bin',0),('partition_table/partition-table.bin','partition-table.bin',0x8000),('m5-fmo-ui-preview.bin',f'fmo-{board}-{version}.bin',0x30000)]:
 data=(build/src).read_bytes()
 if address!=0x8000:
  assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==9,'Expected ESP32-S3 image'
 if address==0x30000:
  assert data[48:80].split(b'\0',1)[0].decode()==version
  assert len(data)<=0x500000
 (out/name).write_bytes(data);parts.append(dict(path=name,address=address,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data)))
assert not subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).strip(), 'Commit candidate sources before packaging'
manifest=dict(board=board,version=version,chip='esp32s3',flash='16MB',status='built-not-device-tested',baseCommit=subprocess.check_output(['git','rev-parse','8de2456'],cwd=root,text=True).strip(),sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),workingTreeChanges=False,originalFirmwareUpgradeCompatible=False,parts=parts)
(out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
(out/'sdkconfig').write_bytes((root/'ui-preview-firmware/sdkconfig.xingzhi').read_bytes())
(out/'README.md').write_bytes((root/'docs/XINGZHI_ADAPTATION.md').read_bytes())
(out/'SHA256SUMS').write_text(''.join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS' and p.suffix!='.zip'))
archive=out/f'fmo-{board}-{version}-candidate.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(out.iterdir()):
  if p.is_file() and p.suffix!='.zip':z.write(p,p.name)
print(archive);print('SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())
