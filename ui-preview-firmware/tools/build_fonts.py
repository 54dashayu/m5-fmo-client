#!/usr/bin/env python3
"""Subset both UI fonts from every literal in main.c; preserve the OFL license."""
from pathlib import Path
import json,re,subprocess,hashlib
root=Path(__file__).resolve().parents[1]
source='\n'.join(f.read_text() for f in [*(root/'main').glob('*.c'),*(root/'main').glob('*.h'),*(root/'main').rglob('*.inc')])
points=sorted(set(range(32,127)) | {ord(c) for s in re.findall(r'"(?:\\.|[^"\\])*"',source) for c in s if ord(c)>127})
font=root/'assets/fonts/NotoSansCJKsc-Bold.otf'
ranges=','.join(str(p) for p in points)
for name,size in [('status_font',12),('font20',20),('history_font',18),('ui_small',11),('ui_call',36),('ui_clock',48)]:
 subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--font',str(font),'--size',str(size),'--bpp','4','--format','lvgl','--no-compress','--no-kerning','--range',('48-58' if name=='ui_clock' else '32-126' if name in ('ui_call','history_font') else ranges),'--lv-font-name',name,'-o',str(root/f'main/fonts/{name}.c')],check=True)
(root/'assets/fonts/coverage.json').write_text(json.dumps({'source_sha256':hashlib.sha256(font.read_bytes()).hexdigest(),'codepoints':points,'count':len(points),'converter':'lv_font_conv@1.5.3'},ensure_ascii=False,indent=2)+'\n')
(root/'main/fonts/coverage.h').write_text('#pragma once\nstatic const uint32_t ui_font_codepoints[] = {'+','.join(str(c) for c in points)+'};\n')
print('Generated both sizes for',len(points),'code points; non-ASCII:', ''.join(chr(c) for c in points if c>127))

# Dynamic relay names are not restricted to firmware literals. Include all
# glyphs provided by Noto in CJK Unified Ideographs plus punctuation.
relay_ranges=ranges+',19968-40959,12288-12351,65281-65374'
subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--font',str(font),'--size','12','--bpp','2','--format','lvgl','--no-compress','--no-kerning','--range',relay_ranges,'--lv-font-name','relay_font','-o',str(root/'main/fonts/relay_font.c')],check=True)

for generated in (root/"main/fonts").glob("*.c"):
 generated.write_text(generated.read_text().rstrip()+"\n")
