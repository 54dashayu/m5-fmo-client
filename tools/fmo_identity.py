"""FMO v4 certificate and login wire-format prototype.

No persistent keys, production CA initialization, or network actions here.
Reference: BG5ESN/fmo-server-authrozier-service src/certs and src/Auth.
"""
import base64
import hashlib
import json

import cbor2
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat


def b64(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def unb64(text):
    return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))


def public_key(key):
    return b64(key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw))


def tbs(cert):
    kind = cert["type"]
    s = cert["subject"]
    if kind == "userCert":
        values = [cert["issuerSn"], s["callsign"].upper(), s["uid"],
                  unb64(s["publicKey"]), cert["iat"], cert["exp"]]
    else:
        i, e = cert["issuer"], cert["extensions"]
        if kind == "rootCA":
            values = [cert["sn"], i["name"], i["email"], s["name"],
                      unb64(s["publicKey"]), e["isCA"], e["pathLen"],
                      e["crl"], e["license"], e["keyId"], cert["iat"], cert["exp"]]
        elif kind == "intermediateCA":
            values = [cert["sn"], i["sn"], i["name"], unb64(i["publicKey"]),
                      s["name"], s["email"], unb64(s["publicKey"]), e["isCA"],
                      e["pathLen"], e["keyId"], e["crl"], e["license"],
                      e["uidRange"]["start"], e["uidRange"]["end"],
                      sorted(e["issuingCountries"]), cert["iat"], cert["exp"]]
        else:
            raise ValueError("unsupported certificate type")
    return cbor2.dumps(["FMO", 4, kind, *values], canonical=True)


def sign_cert(cert, issuer_key):
    cert = dict(cert)
    cert["signatureAlgorithm"] = "Ed25519"
    cert["signature"] = b64(issuer_key.sign(tbs(cert)))
    return cert


def fingerprint(cert):
    return hashlib.sha256(tbs(cert)).digest()


def login(intermediate, user, device_key, server, timestamp):
    """Return MQTT username/password; never log the returned password."""
    if public_key(device_key) != user["subject"]["publicKey"]:
        raise ValueError("device key does not match user certificate")
    role = "user"
    if user["subject"]["uid"] == server["uid"]:
        raise ValueError("client UID must not collide with server UID")
    fields = ["FMO", 4, "serverAuthorizerReqHttp", server["uid"],
              server["callsign"].upper(), server["uid"], role, server["host"],
              server["port"], unb64(server["fingerprint"]), timestamp,
              fingerprint(user)]
    payload = {
        "certPackage": {"intermediateCert": intermediate, "userCert": user},
        "targetCallsign": server["callsign"].upper(), "targetUID": server["uid"],
        "role": role, "targetUrl": server["host"], "targetPort": server["port"],
        "serverFingerprint": server["fingerprint"], "timestamp": timestamp,
        "proof": {"signature": b64(device_key.sign(cbor2.dumps(fields, canonical=True)))},
    }
    return {"username": user["subject"]["callsign"].upper(),
            "password": b64(json.dumps(payload, separators=(",", ":")).encode())}


def ephemeral_test_chain(now):
    """Disposable CA for isolated tests ONLY; keys never written to disk."""
    root_key, issuer_key, device_key = [Ed25519PrivateKey.generate() for _ in range(3)]
    root = sign_cert({
        "sn": 910000001, "type": "rootCA",
        "issuer": {"name": "BH1JSS EPHEMERAL TEST ROOT", "email": ""},
        "subject": {"name": "BH1JSS EPHEMERAL TEST ROOT", "publicKey": public_key(root_key)},
        "extensions": {"isCA": True, "pathLen": 1, "crl": "", "license": "", "keyId": "test-only"},
        "iat": now-60, "exp": now+3600,
    }, root_key)
    intermediate = sign_cert({
        "sn": 910000002, "type": "intermediateCA",
        "issuer": {"sn": root["sn"], "name": root["subject"]["name"], "publicKey": public_key(root_key)},
        "subject": {"name": "BH1JSS TEST ISSUER", "email": "", "publicKey": public_key(issuer_key)},
        "extensions": {"isCA": True, "pathLen": 0, "keyId": "test-only", "crl": "", "license": "",
                       "uidRange": {"start": 910000100, "end": 910000199}, "issuingCountries": ["CN"]},
        "iat": now-60, "exp": now+3600,
    }, root_key)
    user = sign_cert({"type": "userCert", "issuerSn": intermediate["sn"],
                      "subject": {"callsign": "BH1JSS", "uid": 910000101, "publicKey": public_key(device_key)},
                      "iat": now-30, "exp": now+1800}, issuer_key)
    return root, intermediate, user, issuer_key, device_key
