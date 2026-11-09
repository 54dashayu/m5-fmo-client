#!/usr/bin/env python3
"""Subset both UI fonts from every literal in main.c; preserve the OFL license."""
from pathlib import Path
import json,re,subprocess,hashlib
root=Path(__file__).resolve().parents[1]
source='\n'.join(f.read_text() for f in (root/'main').glob('*.c'))
points=sorted(set(range(32,127)) | {ord(c) for s in re.findall(r'"(?:\\.|[^"\\])*"',source) for c in s if ord(c)>127})
font=root/'assets/fonts/NotoSansCJKsc-Bold.otf'
ranges=','.join(str(p) for p in points)
for name,size in [('status_font',12),('font20',20),('ui_small',11),('ui_call',28)]:
 subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--font',str(font),'--size',str(size),'--bpp','4','--format','lvgl','--no-compress','--no-kerning','--range',('32-126' if name=='ui_call' else ranges),'--lv-font-name',name,'-o',str(root/f'main/fonts/{name}.c')],check=True)
(root/'assets/fonts/coverage.json').write_text(json.dumps({'source_sha256':hashlib.sha256(font.read_bytes()).hexdigest(),'codepoints':points,'count':len(points),'converter':'lv_font_conv@1.5.3'},ensure_ascii=False,indent=2)+'\n')
(root/'main/fonts/coverage.h').write_text('#pragma once\nstatic const uint32_t ui_font_codepoints[] = {'+','.join(str(c) for c in points)+'};\n')
print('Generated both sizes for',len(points),'code points; non-ASCII:', ''.join(chr(c) for c in points if c>127))
