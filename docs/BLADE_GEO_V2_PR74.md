# PR74 — Opt-in geocorrelated BLE detection telemetry

## Goal
Connect PR73's classified, GNSS-annotated observation snapshots to the Blade using **sequence-correlated notification fragments**. Do not modify the stable `cardputer-blade-details` environment.

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/geo_frames_test.cpp -o /tmp/callingcard-geo-test
/tmp/callingcard-geo-test
pio run -e cardputer-blade-geo-v2
pio run -e cardputer-blade-geo-v2 -t upload
cd blade2 && ./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Wire format
Sent on the **existing alert characteristic**, one fragment per status tick, nominally 1 second apart:
- `H,seq,type,confidence,rssi,source` — version-2 header, category encoded as digit 0=Flock, 1=ALPR, 2=Axon, 3=Camera, 4=Tracker, 5=Meta, 6=Skimmer. Example: `H,1,0,L,-58,W`.
- `P,seq,latE4,lonE4` — optional valid GNSS position in **signed 1e-4 degrees** (about 11 m latitude resolution). Example: `P,1,423601,-710589`. Missing fix produces **no P**.
- `T,seq,YYMMDDHHMMSS` — optional compact GNSS UTC date/time, ex. `T,1,261026201530`. Missing valid UTC produces **no T**.

Each fragment is fixed/bounded to <20 payload bytes in this first experimental revision and includes the *same sequence*, preventing coordinates from being applied to an unrelated latest observation. Sequence currently wraps at 1000. The Android client accepts legacy `C` and `E` notifications unchanged, and displays geodata on arrival from `P` only when its seq matches the most recent header. It logs the corresponding `T` timestamp.

The opt-in firmware profile sends `H/P/T` for candidates in place of `C`. Generic `E` alerts, detail `D` records, and status retain their original wire formats.

## Limitations and follow-up
- **Not yet a full record with BSSID, BLE address, rule ID or SSID on Blade.** These remain accessible through the Cardputer's local JSONL for Wi-Fi SSIDs and detection rule evidence; BLE callback currently doesn't capture BLE MAC identity.
- Geographic messages are sent as **separate notifications**. There is no full multi-record reassembly buffer or retransmission protocol yet; packet loss may leave partial HUD details. The header is shown immediately. Fragment sequence is bound to one snapshot, and the Blade only attaches a P if seq matches the latest H.
- Only one pending geographic record at a time. Excess candidate attempts during transmission are dropped rather than mis-correlated.
- Old Android Blade apps will ignore H/P/T. Update Android APK when testing the opt-in environment.
- The Cardputer GNSS freshness conditions in CapSensors remain unchanged; no retroactive location enrichment.
- Some extreme coordinate/sequence length combinations can fail the 20-character bound. Such positions are omitted, not truncated or misreported. Future protocol should use an encoded binary or MTU-negotiated characteristic for full precision and identifiers.
- Unencrypted BLE can be observed and spoofed nearby; security is not a prerequisite for functional trials but data isn't authenticated.
- Build and on-device testing pending.

## Acceptance criteria
1. Observe H, then optional P and T with matching sequence on USB Serial.
2. Check Android displays candidate and matched coordinates, without accidentally attaching coordinates from an earlier alert.
3. No GPS fix: H still appears, no P.
4. Confirm the original `cardputer-blade-details` works unchanged.
5. Compare notification cadence and BLE connection stability for ten minutes.
