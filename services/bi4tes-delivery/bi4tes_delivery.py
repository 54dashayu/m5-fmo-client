"""Read-only BI4TES delivery adapter. Never submits applications or handles photos/keys."""
import base64, json, struct, zlib, tempfile, time
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.parse import urlencode
from collections import OrderedDict

ROOT_KEY = "QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc"
_cache = OrderedDict()

def unpack(raw, root, public_key, callsign):
    if not 16 <= len(raw) <= 6144 or raw[:4] != b"FCRD":
        raise ValueError("invalid FCRD header")
    version, size = struct.unpack_from("<II", raw, 4)
    if version != 2 or size != len(raw)-16:
        raise ValueError("invalid FCRD version/length")
    if zlib.crc32(raw[:-4]) != struct.unpack_from("<I", raw, len(raw)-4)[0]:
        raise ValueError("invalid FCRD checksum")
    fields = {}; pos = 12
    while pos < len(raw)-4:
        if pos+3 > len(raw)-4: raise ValueError("truncated TLV")
        tag, size = struct.unpack_from("<BH", raw, pos); pos += 3
        if pos+size > len(raw)-4: raise ValueError("truncated TLV value")
        if tag in (2,3):
            if tag in fields: raise ValueError("duplicate certificate")
            fields[tag] = json.loads(raw[pos:pos+size].decode("utf-8"))
        pos += size
    user, issuer = fields.get(2), fields.get(3)
    if not isinstance(user,dict) or not isinstance(issuer,dict): raise ValueError("missing certificate")
    if root.get("subject",{}).get("publicKey") != ROOT_KEY: raise ValueError("root pin mismatch")
    subject = user.get("subject",{})
    if subject.get("publicKey") != public_key or subject.get("callsign") != callsign:
        raise ValueError("certificate belongs to another identity")
    bundle = {"rootCert":root,"intermediateCert":issuer,"userCert":user}
    # The device performs full Ed25519 chain, scope and lifetime validation.
    if len(json.dumps(bundle,separators=(",",":"))) > 5800: raise ValueError("bundle too large")
    return bundle

def poll(application):
    from ca_enroll import read_request
    with tempfile.NamedTemporaryFile() as f:
        f.write(json.dumps(application).encode()); f.flush(); verified=read_request(f.name)
    key=(verified["publicKey"],verified["callsign"])
    now=time.monotonic()
    if key in _cache and now-_cache[key][0]<30: return _cache[key][1]
    root=json.loads(Path(__file__).with_name("bi4tes-root.json").read_text())
    try:
        url="https://fmo.simy.live/ca/status?"+urlencode({"pk":key[0]})
        with urlopen(Request(url,headers={"Accept":"application/json"}),timeout=2) as response:
            raw=response.read(16385)
        if len(raw)>16384: raise ValueError("response too large")
        data=json.loads(raw)
        state=data.get("state")
        if data.get("revoked") or data.get("expired"):
            result={"state":"rejected"}
        elif state=="ready":
            result={"state":"ready","bundle":unpack(base64.b64decode(data["cert"],validate=True),root,*key)}
        elif state in ("none","pending","rejected"):
            result={"state":state}
        else: result={"state":"error"}
    except Exception:
        result={"state":"error"} # No upstream payloads or identity data in logs.
    _cache[key]=(now,result);_cache.move_to_end(key)
    while len(_cache)>128: _cache.popitem(last=False)
    return result
