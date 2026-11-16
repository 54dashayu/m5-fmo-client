from pathlib import Path
import ast,os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];m=r/'ui-preview-firmware/main';cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';s=(m/'device_web.c').read_text()
tree=ast.parse((r/'tests/test_station_api.py').read_text());pre=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='pre' for t in n.targets))
pre=pre.replace('static struct{bool enabled;int port,passcode;char host[64];}tx_config;', 'typedef struct{bool enabled;int port,passcode;char host[64];}station_tx_config_t;static station_tx_config_t tx_config;')
pre=pre.replace('assert(size==sizeof(station));persisted=*(const station_settings_t*)v;', '(void)v;(void)size;')
host=s[s.index('static bool tx_host_valid('):s.index('static bool tx_busy(')]
fn=s[s.index('static esp_err_t station_broadcast_page('):s.index('static bool station_call_locked(')]
tail=r'''
int main(void){station_defaults(&station);request_t r={0};tx_config.passcode=-1;
input(&r,"{\"enabled\":true,\"host\":\"localhost\",\"port\":14580,\"passcode\":\"12345\"}");station_broadcast_page(&r);assert(!writes);r.authorized=true;station_broadcast_page(&r);assert(!ok(&r)&&!writes);
station.has_location=true;station.frequency=438.5;fail_commit=1;station_broadcast_page(&r);assert(!ok(&r)&&!tx_config.enabled&&tx_config.passcode==-1);fail_commit=0;station_broadcast_page(&r);assert(ok(&r)&&tx_config.enabled&&tx_config.passcode==12345);
input(&r,"{\"enabled\":false,\"host\":\"localhost\",\"port\":14580,\"passcode\":\"\"}");station_broadcast_page(&r);assert(ok(&r)&&!tx_config.enabled&&tx_config.passcode==12345);
input(&r,"{\"action\":\"send\"}");station_broadcast_page(&r);assert(!ok(&r));
const char*bad[]={"-1","32768","1x","123456"};for(int i=0;i<4;i++){char j[200];snprintf(j,sizeof(j),"{\"enabled\":false,\"host\":\"localhost\",\"port\":14580,\"passcode\":\"%s\"}",bad[i]);input(&r,j);int old=writes;station_broadcast_page(&r);assert(!ok(&r)&&writes==old);}
cJSON_Delete(r.input);cJSON_Delete(r.output);puts("PASS broadcast API: auth, prerequisites, commit failure, passcode bounds, keep existing secret, disable and manual-send guard");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.c';p.write_text(pre+host+fn+tail);subprocess.run(['cc','-I'+str(m),'-I'+str(cj),'-I'+str(r/'ui-preview-firmware/components/fmo_board'),str(p),str(m/'station_settings.c'),str(cj/'cJSON.c'),'-lm','-o',td+'/t'],check=True);subprocess.run([td+'/t'],check=True)
