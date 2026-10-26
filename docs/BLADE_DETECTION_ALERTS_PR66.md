# PR66 — Classified detections on the Blade HUD

Uses the existing, unencrypted **alert characteristic** `...0003` and the existing Blade notification subscription. The legacy aggregate event `E,<seq>,<W|B|L>` remains valid. Candidate detection packets use:

```
C,<TYPE>,<L|M|H>,<RSSI>,<W|B>
```
For example `C,FLOCK,M,-58,W` or `C,TRACKER,L,-73,B`. Fields are bounded and contain **no device identifiers, GPS coordinates, SSIDs, keys, or raw advertisement data**. This is a confidence label, not probability or proof. Android renders `POSSIBLE FLOCK / MEDIUM / -58 dBm / Wi-Fi` in the existing HUD alert slot. Existing generic events and detail telemetry are retained.

## Rate limiting
A fixed 8-slot hash-key cooldown suppresses the same category + rule + source for 30 seconds, without tracking device identifiers. A single pending detection slot prevents unbounded buffering. Different rules that collide in a slot can evict one another; this is intended as best-effort noise control, not strict per-device deduplication. An unsent detection is not overwritten by another candidate. Unbonded GATT is unchanged.

## Manual verification
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/detection_cooldown_test.cpp -o /tmp/callingcard-detection-cooldown
/tmp/callingcard-detection-cooldown
pio run -e cardputer-blade-details
pio run -e cardputer-blade-details -t upload
cd blade2 && ./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -v time -s CallingcardBlade
```

Use a **controlled BLE test advertisement** with the name `Tile Pro` or a controlled Wi-Fi test AP named `flock-test` to exercise name-based rules. These are *spoofable test fixtures* and **must not be treated as real hardware identifications**. Test normal status updates and generic event alerts as a regression check.

No additional firmware scanning, permissions, bonding, security changes or Bluetooth characteristics. This change is intentionally small to preserve the working Blade link. Build and hardware testing pending.
