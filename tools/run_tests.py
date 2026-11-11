#!/usr/bin/env python3
"""Offline regression suite. Requires a C compiler and IDF_PATH for cJSON."""
import os, subprocess, sys, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
if not os.environ.get('IDF_PATH'):
 raise SystemExit('Set IDF_PATH to your ESP-IDF v5.5.1 checkout (cJSON verifier test).')
for name in ('side_leds','discovery','relay_evidence','relay_delete','voice_wire','mqtt_bounded_enqueue','mqtt_partial_receive','mic_calibration','device_cert','identity_transition','wifi_profiles','usb_recovery'):
 subprocess.run([sys.executable,str(root/'tests'/f'test_{name}.py')],check=True,cwd=root)
with tempfile.TemporaryDirectory() as td:
 for name,extra in [('controls',['buttons.c']),('voice_tx',[]),('voice_dsp',[]),('utf8_name',[]),('voice_dedup',[]),('call_history',[]),('display_idle',[]),('battery',[]),('roger_beep',['voice_wire.c'])]:
  out=str(Path(td)/name)
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(root/'tests'/f'test_{name}.c'),*[str(root/'ui-preview-firmware/main'/p) for p in extra],'-o',out],check=True)
  subprocess.run([out],check=True)
print('All offline regressions passed.')
