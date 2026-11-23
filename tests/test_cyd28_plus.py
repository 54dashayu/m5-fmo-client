"""Offline ESP32-2432S028R Plus calibration, bounded flash album and compiled-target gate."""
from pathlib import Path
import subprocess,tempfile,csv,json
r=Path(__file__).resolve().parents[1];fw=r/'ui-preview-firmware'
with tempfile.TemporaryDirectory() as td:
 for name,flags,sources,inc in [
  ('touch',[],['tests/test_cyd28_plus.c'],'components/lv_porting'),
  ('store',['-DSSTV_HOST_TEST','-DSSTV_HOST_CAPACITY=917504','-DCONFIG_IDF_TARGET_ESP32','-DCONFIG_FMO_BOARD_CYD28_PLUS'],['tests/test_cyd28_plus_store.c','ui-preview-firmware/main/sstv_store.c'],'main')]:
  out=str(Path(td)/name);subprocess.run(['cc','-Wall','-Wextra','-Werror',*flags,'-I'+str(fw/inc),*[str(r/s) for s in sources],'-o',out],check=True);subprocess.run([out],check=True)
 end=0x9000;parts={}
 for row in csv.reader((fw/'partitions.cyd28_plus.csv').read_text().splitlines()):
  if not row or row[0].startswith('#'):continue
  name,typ,sub,offset,size,*_=map(str.strip,row);offset=int(offset,0);size=int(size,0)
  assert offset>=end and offset+size<=4*1024*1024
  if typ=='app':assert offset%0x10000==0
  end=offset+size;parts[name]=(offset,size)
 assert end==4*1024*1024 and parts['nvs']==(0x9000,0x6000)
 build=r/'build-cyd28_plus';cfg=(build/'config/sdkconfig.json')
 if cfg.exists():
  c=json.loads(cfg.read_text());assert c['FMO_BOARD_CYD28_PLUS'] and not c.get('FMO_BOARD_M5_FIRE') and not c.get('SPIRAM');assert c['ESPTOOLPY_FLASHSIZE']=='4MB'
  app=build/'m5-fmo-ui-preview.bin';assert app.stat().st_size<=parts['factory'][1]
  web=(build/'esp-idf/main/web.html').read_text();assert "const tfSupported='0'" in web and 'ESP32-2432S028R Plus 设备管理' in web
 print('PASS 4MB partition bounds, NVS preservation, no PSRAM, actual app fit, internal-only web')
