# Blade observation lifecycle — PR 40

This **Android-only** stage improves recency and history without transmitting hardware addresses over today's unauthenticated BLE channel. The existing Cardputer firmware and GATT UUIDs/payloads are unchanged.

## Semantics

- Wi-Fi = recent if received within 120 seconds; BLE = recent if received within 180 seconds. These are conservative UI heuristics to reflect their different survey cadences, **not** statements that an AP/device is currently online.
- Older rows are dimmed and can remain in the app's bounded list for up to 15 minutes since last observation. Entries older than that are discarded opportunistically when a new observation arrives; the view also filters them immediately, even without incoming observations. Details show RECENT/OLD and last-seen age.
- The scanner selection follows its existing **name + channel** (Wi-Fi) or **advertised name** (BLE) while new observations reorder the list.
- **Menu key** toggles between RECENT and ALL retained entries. Gesture mapping for Menu depends on Blade firmware and may need an accessible touchpad gesture in a follow-up. Left/right/up/down/center/back behavior remains.
- RSSI histories persist only as long as the retained record does. The channel histogram keeps its own 120-second raw observation history.
- Details label matching as NAME-BASED MATCH or IDENTITY UNKNOWN. Matching **is not** physical transmitter identity. Two Wi-Fi APs on the same SSID and channel, or BLE devices with the same advertised name, remain ambiguous. Missing/hidden names are not deduplicated.

## Why no new device IDs yet?

BSSID or BLE hardware addresses are sensitive tracking data and the current GATT channel permits unauthenticated reads. Stable private pseudonyms would also require careful per-session keyed hashing, key lifecycle, bonding, and Android authorization. This PR intentionally does **not** pretend to solve unique hardware identity or add such identifiers.

## Test

1. Merge PR and pull main; rebuild/reinstall Android `blade2/` APK. **No Cardputer reflash.**
2. Confirm scanner pages, swipe controls, overview counters and detail RSSI graphs still work.
3. Leave a Wi-Fi/BLE source unobserved past its age threshold and confirm OLD/dim status; a new matching observation should become recent again and refresh RSSI history.
4. If the Blade exposes an Android Menu key, use it to switch recent-only and all retained entries. If there is no Menu gesture, the default ALL view still labels older observations and a future iteration can add a touchpad-accessible toggle.
5. Confirm selected rows remain stable through list reorder where the same name-based key can be matched; test anonymous entries separately.
6. Verify BLE link and SD logging are unaffected. Report issues with the opt-in Android-only change.

Not compiled or hardware-tested by assistant.
