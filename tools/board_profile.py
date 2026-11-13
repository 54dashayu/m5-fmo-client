#!/usr/bin/env python3
"""Validate a board description and emit a build-local header, never touch hardware."""
import argparse,hashlib,json,re
from pathlib import Path
PINS=('lcd_mosi','lcd_sclk','lcd_dc','lcd_cs','lcd_reset','backlight','power_hold','charging','battery','mic_bclk','mic_ws','mic_din','spk_bclk','spk_ws','spk_dout','key_a','key_b','key_c')
def validate(p):
 if p.get('schema')!=1 or p.get('chip')!='esp32s3':raise ValueError('Only schema 1 / ESP32-S3 supported')
 if not re.fullmatch(r'[a-z0-9][a-z0-9.-]{0,55}',p.get('id','')):raise ValueError('Invalid profile id')
 if p.get('flash_mb')!=16 or p.get('psram_mb')!=8:raise ValueError('This build currently requires N16R8 memory and its partition map')
 if p.get('audio')!='i2s-simplex' or p.get('battery_calibration')!='xingzhi-v1':raise ValueError('Unsupported audio/battery driver')
 if p.get('power_key') is not None:raise ValueError('Power key GPIO is not verified; do not guess an input or power-off sequence')
 d=p['display'];pins=p['pins']
 if d.get('driver')!='st7789':raise ValueError('Only ST7789 SPI driver implemented; add and validate a driver before selecting another model')
 if not isinstance(d.get('model'),str) or not d['model'] or len(d['model'])>80:raise ValueError('Invalid display model')
 if (d.get('width'),d.get('height')) not in [(240,240),(240,320),(320,240)]:raise ValueError('Supported canvas sizes: 240x240, 240x320, 320x240')
 for k in ['invert','bgr','swap_xy','mirror_x','mirror_y','backlight_inverted']:
  if type(d.get(k)) is not bool:raise ValueError('Expected boolean: '+k)
 for k,lo,hi in [('spi_mode',0,3),('clock_hz',1000000,40000000),('gap_x',0,80),('gap_y',0,80)]:
  if type(d.get(k)) is not int or not lo<=d[k]<=hi:raise ValueError('Invalid display parameter: '+k)
 # Controller memory remains 240x320; swapping XY changes logical dimensions.
 physical=(d['height'],d['width']) if d['swap_xy'] else (d['width'],d['height'])
 if physical[0]+d['gap_x']>240 or physical[1]+d['gap_y']>320:raise ValueError('ST7789 memory bounds exceeded; check swap_xy/gaps')
 if set(pins)!=set(PINS):raise ValueError('Pin map must explicitly name all supported peripherals')
 allowed=set(range(0,22))|set(range(38,49));allowed-={19,20} # keep native USB, reserve octal memory pins
 for k,v in pins.items():
  if type(v)is not int or v not in allowed:raise ValueError('Reserved or invalid GPIO: '+k)
  if v==46 and k not in ['key_a','key_b','key_c','charging','battery','mic_din']:raise ValueError('GPIO46 is input-only')
 if len(set(pins.values()))!=len(pins):raise ValueError('Overlapping GPIO assignments are not supported by these drivers')
 if not 11<=pins['battery']<=18:raise ValueError('Battery input must be ADC2 GPIO11..18')
 return p

def render(path):
 raw=Path(path).read_bytes();p=validate(json.loads(raw));d=p['display']
 lines=['#pragma once','// Generated in the build directory; edit the board JSON, not this file.']
 def define(k,v):lines.append('#define '+k+' '+(json.dumps(v,ensure_ascii=True) if isinstance(v,str) else str(int(v))))
 define('FMO_BOARD_ID',p['id']);define('FMO_PROFILE_HASH',hashlib.sha256(raw).hexdigest()[:16]);define('FMO_SCREEN_MODEL',d['model'])
 define('FMO_WIDTH',d['width']);define('FMO_HEIGHT',d['height'])
 for k,v in p['pins'].items():
  if k=='battery':continue
  define('FMO_'+('CHARGING_GPIO' if k=='charging' else k.upper()),v)
 lines.append('#define FMO_BATTERY_CHANNEL ADC_CHANNEL_'+str(p['pins']['battery']-11))
 for k in ['spi_mode','clock_hz','invert','bgr','swap_xy','mirror_x','mirror_y','gap_x','gap_y','backlight_inverted']:define('FMO_LCD_'+k.upper(),d[k])
 return '\n'.join(lines)+'\n'
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('profile');ap.add_argument('output');a=ap.parse_args()
 try:text=render(a.profile)
 except (ValueError,KeyError,TypeError) as e:ap.error(str(e))
 out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True)
 if not out.exists() or out.read_text()!=text:out.write_text(text)
