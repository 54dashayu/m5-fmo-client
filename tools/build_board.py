#!/usr/bin/env python3
"""Build one FMO board in an isolated directory. Never opens a device or flashes."""
import argparse, os, subprocess, sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('board',choices=['xingzhi','m5']);a=p.parse_args()
root=Path(__file__).resolve().parents[1];fw=root/'ui-preview-firmware'
idf=os.environ.get('IDF_PATH')
if not idf:raise SystemExit('Activate ESP-IDF v5.5.1 first.')
build=root/('build-xingzhi' if a.board=='xingzhi' else 'build-m5-regression')
build.mkdir(exist_ok=True)
if a.board=='xingzhi':
 config=fw/'sdkconfig.xingzhi';defaults=fw/'sdkconfig.xingzhi.defaults';target='esp32s3'
else:
 config=build/'sdkconfig';defaults=fw/'sdkconfig.defaults';target='esp32'
 if not config.exists():config.write_bytes((fw/'sdkconfig').read_bytes())
subprocess.run([sys.executable,str(Path(idf)/'tools/idf.py'),'-C',str(fw),'-B',str(build),f'-DIDF_TARGET={target}',f'-DSDKCONFIG={config}',f'-DSDKCONFIG_DEFAULTS={defaults}','build'],check=True)
