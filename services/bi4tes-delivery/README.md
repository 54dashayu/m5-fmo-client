# BI4TES public certificate delivery adapter

The M5 browser sends the user's license photo directly to `https://fmo.simy.live/ca/apply` after consent. This adapter **only queries** `/ca/status` for a device-signed request and converts its public FCRD v2 certificate to JSON. No photos, upstream applications, private keys or signing operations pass through this adapter.

Install `bi4tes_delivery.py` and `bi4tes-root.json` beside the existing enrollment `service.py` (which provides `ca_enroll.read_request` through its tools path). Add:

```python
from bi4tes_delivery import poll as bi4tes_poll
# In the existing /m5-ca/apply POST handler:
self.reply(200, bi4tes_poll(j) if j.get('provider') == 'bi4tes' else store.submit(j))
```

The existing BH1JSS application path is unchanged. Status queries have a 2-second upstream timeout, 16 KiB response cap and bounded 30-second cache. The fixed HTTPS upstream uses normal system TLS verification. No caller-supplied URL is accepted. Device signatures are verified before querying. The pinned public root is not a signing key.

The M5 independently verifies the complete Ed25519 chain, expected provider root, lifetime, callsign, UID scope and its own public-key binding. Conversion success is not authentication success. Unknown FCRD fields are ignored, never treated as private keys.

Run `python3 tests/test_bi4tes_delivery.py` from the repository root. For deployment, back up the current service, install the two adapter files, patch only the handler/import above, compile-check, restart and verify the existing service plus a signed BI4TES status request. Roll back the service backup if checks fail.
