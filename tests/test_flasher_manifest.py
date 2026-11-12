"""Execute the real flasher version allowlist against stable and candidate names."""
from pathlib import Path
import subprocess,re,json
s=(Path(__file__).resolve().parents[1]/'web-flasher/app.js').read_text()
pattern=re.search(r'if\(!(/[^\n]+?/).test\(manifest.version\)',s)[1]
code='const assert=require("node:assert/strict");const valid='+pattern+';'
code+='for(const v of '+json.dumps(['0.7.26','0.7.27','0.7.27-dev','0.7.27-rc1','0.7.27-rc12'])+')assert(valid.test(v),v);'
code+='for(const v of '+json.dumps(['0.7.27-rc0','0.7.27-rc','0.7.27-rc1/../../secret','0.7.27-rc01','other','0.7.27-beta'])+')assert(!valid.test(v),v);'
subprocess.run(['node','-e',code],check=True)
print('PASS actual flasher version gate: stable/dev/numbered RC and invalid/path rejection')
