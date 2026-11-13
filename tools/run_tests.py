#!/usr/bin/env python3
"""Offline regression suite. Requires a C compiler and IDF_PATH for cJSON."""
import os, subprocess, sys, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
if not os.environ.get('IDF_PATH'):
 raise SystemExit('Set IDF_PATH to your ESP-IDF v5.5.1 checkout (cJSON verifier test).')
for name in ('sstv_mount','sstv_upload','resource_metrics','xingzhi_flush','home_refresh','voice_sstv_jitter','sstv_stream','sstv_stream_preview','sstv_preview','sstv_rx','sstv_ui','sstv_api','auto_profiles','auto_profiles_web','discovery_auto_roots','board_profile','ui_settings','flasher_manifest','flasher_flow','identity_ui','root_profiles','discovery_identity_change','profile_transition','bi4tes_delivery','voice_identity','side_leds','management_code','remember_login','discovery_select','discovery_web','discovery_policy','discovery_candidates','discovery_add','trusted_catalog','trust_priority','discovery','relay_pagination','relay_storage','voice_wire','mqtt_bounded_enqueue','mqtt_partial_receive','mic_calibration','device_cert','enrollment_jobs','identity_transition','wifi_profiles','usb_buffer','usb_recovery'):
 subprocess.run([sys.executable,str(root/'tests'/f'test_{name}.py')],check=True,cwd=root)
with tempfile.TemporaryDirectory() as td:
 for name,extra in [('sstv_store',['sstv_store.c']),('sstv',['sstv_decoder.c','voice_wire.c']),('auto_ca',[]),('digital_audio',[]),('controls',['buttons.c']),('key_layout',['buttons.c']),('screen_sleep',[]),('voice_tx',[]),('voice_dsp',[]),('voice_mic_metrics',[]),('utf8_name',[]),('voice_dedup',[]),('voice_rx',[]),('call_history',[]),('display_idle',[]),('call_indicator',[]),('battery',[]),('roger_beep',['voice_wire.c'])]:
  out=str(Path(td)/name)
  subprocess.run(['cc',*(['-DSSTV_HOST_TEST'] if name=='sstv_store' else []),'-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(root/'tests'/f'test_{name}.c'),*[str(root/'ui-preview-firmware/main'/p) for p in extra],'-o',out,*(['-lm'] if name in ('digital_audio','sstv','voice_mic_metrics') else [])],check=True)
  subprocess.run([out],check=True)
with tempfile.TemporaryDirectory() as td:
 subprocess.run(['cc','-DSSTV_HOST_TEST','-DCONFIG_FMO_BOARD_XINGZHI','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(root/'tests/test_sstv_partial_store.c'),str(root/'ui-preview-firmware/main/sstv_store.c'),'-o',td+'/partial'],check=True)
 subprocess.run([td+'/partial'],check=True)
with tempfile.TemporaryDirectory() as td:
 subprocess.run(['cc','-DSSTV_HOST_TEST','-DCONFIG_IDF_TARGET_ESP32','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),str(root/'tests/test_sstv_stream_store.c'),str(root/'ui-preview-firmware/main/sstv_store.c'),'-o',td+'/stream'],check=True)
 subprocess.run([td+'/stream'],check=True)
 cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),'-I'+str(cjson),str(root/'tests/test_usb_json_stream.c'),str(cjson/'cJSON.c'),'-o',td+'/usb-json'],check=True)
 subprocess.run([td+'/usb-json'],check=True)
print('All offline regressions passed.')
