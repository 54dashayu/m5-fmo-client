from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];m=r/'ui-preview-firmware/main';cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 for flags in [[],['-DCONFIG_FMO_BOARD_XINGZHI=1']]:
  subprocess.run(['cc','-Wall','-Wextra','-Werror',*flags,'-I'+str(m),'-I'+str(cj),'-I'+str(r/'ui-preview-firmware/components/fmo_board'),str(r/'tests/test_station_settings.c'),str(m/'station_settings.c'),str(cj/'cJSON.c'),'-lm','-o',td+'/test'],check=True)
  subprocess.run([td+'/test'],check=True)
 # ESP-IDF exposes board choice through sdkconfig.h, not compiler -D flags.
 Path(td,'sdkconfig.h').write_text('#define CONFIG_FMO_BOARD_XINGZHI 1\n')
 Path(td,'prefix.c').write_text('#include "station_settings.h"\n#include <assert.h>\n#include <string.h>\nint main(void){char out[13];station_display_call(out,"BH1JSS",true);assert(!strcmp(out,"E.BH1JSS"));}\n')
 subprocess.run(['cc','-DESP_PLATFORM=1','-I'+td,'-I'+str(m),'-I'+str(cj),'-I'+str(r/'ui-preview-firmware/components/fmo_board'),td+'/prefix.c',str(m/'station_settings.c'),str(cj/'cJSON.c'),'-lm','-o',td+'/prefix'],check=True)
 subprocess.run([td+'/prefix'],check=True)
 print('PASS S3 prefix uses sdkconfig without command-line board defines')
