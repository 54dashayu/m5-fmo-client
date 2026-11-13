from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/sstv_upload.inc').read_text();a=s.index('static bool wall_text');b=s.index('static esp_err_t gallery_wall_http');functions=s[a:b]
pre='''#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "cJSON.h"
static const char*js(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
'''
test='''
int main(void){cJSON*j=cJSON_CreateObject();assert(!wall_payload(j,false));cJSON_AddStringToObject(j,"image","data:image/png;base64,AAAAAAAAAAAA");cJSON_AddStringToObject(j,"callsign","BH1JSS");cJSON_AddStringToObject(j,"received","2026-11-13T14:00");cJSON_AddBoolToObject(j,"consent",false);assert(!wall_payload(j,false));cJSON_ReplaceItemInObject(j,"consent",cJSON_CreateTrue());assert(wall_payload(j,false));cJSON_ReplaceItemInObject(j,"callsign",cJSON_CreateString("bad<script>"));assert(!wall_payload(j,false));cJSON_ReplaceItemInObject(j,"callsign",cJSON_CreateString("BH1JSS"));cJSON_ReplaceItemInObject(j,"image",cJSON_CreateString("https://private/image.png"));assert(!wall_payload(j,false));cJSON_Delete(j);j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id","test-id");assert(!wall_payload(j,true));cJSON_AddStringToObject(j,"delete_token","test-only-capability");assert(wall_payload(j,true));cJSON_Delete(j);puts("PASS upload consent, callsign, inline image requirement and withdrawal capability validation");}
'''
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+functions+test);subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cjson),str(p),str(cjson/'cJSON.c'),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
assert 'crt_bundle_attach=esp_crt_bundle_attach' in s and 'disable_auto_redirect=true' in s
assert s.count('https://space.bh1jss.net/sstv-wall/api/')==3
