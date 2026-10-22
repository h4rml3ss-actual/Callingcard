# Blade 2 HUD v1 — PR 33

This is an **Android-only** display improvement. It preserves the v1 GATT ASCII protocol and the proven `cardputer-blade-coexist` firmware. No firmware rebuild/upload needed.

## Interface
- High-contrast, monospace centered HUD for compact Blade 2 viewing.
- Wi-Fi, BLE and LoRa counters, GPS fix state and SD recording state.
- An asterisk (*) beside a counter means its value **changed in the most recently received telemetry payload**, not necessarily that a new distinct device was found. Wi-Fi and BLE counts reflect latest scan counts, so changes can be up or down. LoRa counter is modulo 1000.
- Visible telemetry freshness: **LIVE** if last payload <3s old, **DELAYED** from 3–10s, **STALE** after 10s. These are local app receipt-time checks, not synchronized device clock measurements. Stale values stay visible but the color changes to warn they're outdated.
- The existing Android GATT discovery and reconnect logic remains intact.

## Install / validation

1. Merge PR #33 and pull main to Mac.
2. Open `blade2/` in Android Studio; build a debug APK.
3. Connect Blade 2 via USB/ADB; install the new app with the normal Android Studio Run button or:

       ~/Library/Android/sdk/platform-tools/adb install -r blade2/app/build/outputs/apk/debug/app-debug.apk

4. Launch Callingcard HUD with the already working `cardputer-blade-coexist` firmware.
5. Confirm live counters and GPS/SD status; check asterisks appear only when the corresponding counter changes.
6. Temporarily stop Cardputer telemetry or move out of range; if the BLE connection remains up but packets stop, watch LIVE -> DELAYED -> STALE and then resume. If connection drops, the app displays DISCONNECTED and reconnects using existing logic.
7. Inspect layout readability through the optics; report font scale or cropping and adjust after on-device feedback.

The HUD doesn't yet display device identities, precise coordinates or actionable security alerts. BLE GATT remains unauthenticated. This source change has not been compiled or physically validated by the assistant.
