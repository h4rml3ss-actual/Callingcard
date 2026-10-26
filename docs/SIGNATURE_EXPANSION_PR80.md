# PR80 — Additional conservative Wi-Fi device signatures

PR80 adds Axon body-camera pairing-mode SSID families `AB2-`, `AB3-`, `AB4-`; vendor-associated Axon/Taser OUIs `00:25:DF`, `E4:05:40`; and camera-vendor OUIs for Verkada (`E0:A7:00`), Avigilon Alta (`70:1A:D5`), and Axis (`00:40:8C`, `B8:A4:4F`).

Source and provenance review: [SquachWatch-CYD detection provenance](https://github.com/skizzophrenic/SquachWatch-CYD/blob/master/docs/DETECTIONS.md), which references Axon Evidence guidance and IEEE MA-L registry checks. Identifier matching provides **candidate evidence** only. Vendor ownership does not prove a body-camera or IP camera: diverse equipment can share allocations; MACs/SSIDs may be spoofed. Prefixes and assignments should be audited periodically.

### Scoring
- Axon pairing-mode SSID: moderate indicator, confidence capped at MEDIUM.
- Axon registered MAC prefix alone: LOW.
- Combined Axon SSID + Axon prefix: MEDIUM maximum (same transmitter; not independent confirmed evidence).
- Commercial camera manufacturer MAC prefix alone: LOW.
- Existing Flock and ALPR candidate behavior unchanged. We explicitly exclude generic chipset OUIs and historical Sonos false positives.
- No new BLE manufacturer payload rules or extra ALPR shared-vendor prefixes without device-specific evidence; their absence is deliberate.

### Validation
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/wifi_signatures_test.cpp -o /tmp/callingcard-wifi80
/tmp/callingcard-wifi80
pio run -e cardputer-blade-evidence-v3
```
Use a deliberately synthetic test SSID `AB3-TEST` to verify candidate production, and confirm that `AXONOMY`, generic ESP32 OUIs, and random/local addresses do not trigger surveillance-device matches. Do not interpret test beacons as verified field detections. Blade GATT transport, SD writing, Wi-Fi/BLE scan cadence and existing GNSS tagging are unchanged.

Build and hardware testing pending.
