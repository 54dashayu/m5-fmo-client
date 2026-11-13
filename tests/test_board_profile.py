from pathlib import Path
import copy,json,sys,tempfile
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from board_profile import validate,render
path=r/'boards/xingzhi-cube-1.54tft-wifi.json';p=json.loads(path.read_text());validate(p)
s=render(path)
for line in ['#define FMO_KEY_A 39','#define FMO_KEY_B 0','#define FMO_KEY_C 40','#define FMO_LCD_MOSI 10','#define FMO_POWER_HOLD 21','#define FMO_BATTERY_CHANNEL ADC_CHANNEL_6','#define FMO_WIDTH 240','#define FMO_HEIGHT 240']:assert line in s
mutations=[lambda q:q['pins'].update(key_a=19),lambda q:q['pins'].update(key_a=33),lambda q:q['pins'].update(key_a=40),lambda q:q.update(power_key=12),lambda q:q['display'].update(driver='unknown'),lambda q:q['display'].update(width=480),lambda q:q.update(flash_mb=4),lambda q:q['display'].update(gap_y=81),lambda q:q['display'].update(width=320,height=240),lambda q:q['display'].update(invert='false')]
for mutate in mutations:
 q=copy.deepcopy(p);mutate(q)
 try:validate(q)
 except ValueError:pass
 else:raise AssertionError('unsafe profile accepted')
q=copy.deepcopy(p);q['display'].update(width=240,height=320);validate(q)
q['display'].update(width=320,height=240,swap_xy=True);validate(q)
with tempfile.TemporaryDirectory() as td:
 f=Path(td)/'profile.json';f.write_text(json.dumps(q));assert '#define FMO_WIDTH 320' in render(f)
print('PASS profile generation preserves board pins; rejects USB/memory conflicts, duplicate pins, guessed power key and unsupported displays')
