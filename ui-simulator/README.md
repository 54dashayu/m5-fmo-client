# M5 FMO UI simulator

Run from m5-fmo-client: `python3 -m http.server 8773 --bind 127.0.0.1`
Open http://127.0.0.1:8773/ui-simulator/ .

320x240 HTML design preview, with local sample state only. No device or server API requests. Short/long A/B/C interactions and basic menus mirror current firmware; received audio, meter, relay IDs and networking are simulated. The homepage design has been ported to LVGL; this simulator remains a design preview rather than hardware validation. Uses existing licensed project font.

Verified in browser: B toggle, long C menu, relay selection, long A home, RX transmit guard.
