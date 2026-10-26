# PR78 — Blade detection evidence details

Extends the existing fifth DETECTIONS HUD page. The Android app now attaches validated, sequence-matched PR74/PR77 fragments to the **selected classified alert record** and presents:
- Candidate category, confidence, latest RSSI, Wi-Fi/BLE source
- Reassembled Wi-Fi BSSID or BLE advertiser address, when available
- Detection signature rule identifier, when available
- Correlated GNSS coordinates and UTC date/time, when available
- Grouped alert count and peak RSSI

The Android receiver only updates the active entry whose PR74 H sequence matches incoming P/T/X. Existing PR77 20-second X-fragment timeout applies. A new H resets the active sequence and previous in-flight evidence cannot fill in the next entry. Older legacy C notifications still show their category but mark identifier/evidence/location **unavailable**.

## Testing

```bash
cd blade2
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -v time -s CallingcardBlade
```

Use the opt-in `cardputer-blade-evidence-v3` Cardputer profile from PR77 and a controlled test emitter. Open the fifth HUD page, select the latest candidate, and verify the identifier, signature rule, UTC and GPS. With no GNSS fix, location remains unavailable. Check a second alert arriving between fragment groups; fragments associated with an older sequence must not enrich the newer alert. Check the original C/E notifications and tabs after reconnecting.

## Limitations

- The PR72 history remains grouped by category/source (not uniquely by physical device). These richer fields apply to the **most recently updated group**, and are not yet persisted as part of historical records; a restart restores summary counts, not enriched fields.
- Advertiser addresses can be spoofed/randomized. The UI always labels candidate matches as possible, not verified.
- GPS is the location of the **Cardputer receiver at observation time**, not necessarily the transmitter's location.
- No GATT protocol, radio scheduling or encryption changes in PR78.
- Gradle build and hardware validation are pending.
