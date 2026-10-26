# PR72 — Bounded Blade detection history and alert aggregation

Extends PR71's DETECTIONS HUD page **only**. No firmware, BLE GATT characteristics, pairing behavior, scanning cadence, or radio changes.

## Behavior
- Groups classified candidate notifications by **category + source** when the preceding matching alert arrived within 120 seconds.
- Grouping is an **alert summary, not device deduplication**: because the existing privacy-preserving PR66 wire format omits MACs and per-device identifiers, unrelated transmitters may be grouped together.
- Tracks first/last receipt timestamps, number of grouped candidate sightings (capped at 9,999), latest RSSI and strongest (least negative) RSSI, as well as **highest reported confidence** seen within the group. Confidence is not independently recalculated or proof of identity.
- Keeps the most recent 24 groups, with 24-hour retention, as local app-private SharedPreferences JSON. Entries are loaded on HUD construction and pruned on subsequent arrivals and startup.
- Does not save SSIDs, BLE addresses, device labels, coordinates, or raw advertisement bytes.
- Displays the number of grouped sightings and peak RSSI on the DETECTIONS page; detailed view clarifies that grouping is not unique device identification.

## Test plan
```sh
cd blade2
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```
1. Pair/connect using the working **unencrypted** Cardputer Blade details environment; ensure Wi-Fi/BLE status, details and generic E alerts still flow.
2. Trigger a controlled detection candidate (for example, test SSID `flock-test`, whose name is trivially spoofed and **not proof of a camera**).
3. Deliver repeated alerts within 120 seconds; observe **one grouped entry** with increasing sightings, new latest signal and unchanged/raised maximum confidence.
4. After more than 120 seconds between observations, a new group should appear; verify older group remains separately visible.
5. Force-stop and relaunch the Android app: groups should restore from private app preferences.
6. Verify records older than 24 hours are discarded on restart or next candidate.
7. Confirm the Bluetooth/Android HUD health remains stable and notification latency is unchanged.

## Limitations
No cross-device tracking is possible or desirable with the current GATT alert payload. The Cardputer PR66 cooldown may suppress many repeated candidate messages before they reach Android; displayed sighting count reflects **received classified alerts only**, not underlying RF packets. Confidence retained is maximum *reported* confidence, not evidence-weighted longitudinal probability. Persistent summary stays on the Blade; clearing application data clears history.

Build, emulator and physical hardware verification are pending.
