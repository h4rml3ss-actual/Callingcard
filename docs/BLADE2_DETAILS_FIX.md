# Blade 2 scanner detail fixes — PR 36

## Fixes

1. **BLE advertised devices not visible:** BLE callbacks populate a main-loop prequeue, and `drainBleSamples()` forwarded directly to the event pipeline rather than `addObservation()`. PR 35's `blade.queueDetail('B',...)` therefore never ran. This PR queues BLE details explicitly while draining the prequeue, still outside of interrupt/callback context.
2. **Truncated Wi-Fi SSIDs:** The detail wire payload previously used `%.8s` and a 20-byte buffer. The opt-in detail stream now supports 23-byte labels and a 48-byte payload buffer. Both sides request/support ATT MTU 100; scanner names are clipped by measured graphical width only if they don't fit. The hardware observation snapshot itself contains only a 24-byte label, so longer SSIDs cannot be recovered in this revision.
3. **Compatibility:** Existing aggregate telemetry and alert UUIDs remain unchanged. The firmware environment remains `cardputer-blade-details`; previously validated `cardputer-blade-alerts` and `cardputer-blade-coexist` remain as fallbacks.

## On-device validation

- Merge, pull, flash `pio run -e cardputer-blade-details -t upload`.
- Build/reinstall `blade2/` Android APK.
- Confirm live status, event alerts and touchpad swipe navigation.
- Check Android Logcat `CallingcardBlade` for `negotiated MTU=100` (the negotiated value may differ). The longer labels require a negotiated MTU above the default 23. If MTU negotiation fails, expect detail notifications to be truncated or absent; report the actual negotiated value.
- Wait for a BLE survey (the coexistence profile surveys infrequently, no more than one two-second window each 30 seconds after link establishment) and confirm BLE advertisements appear on the BLE page; verify Wi-Fi labels are longer than eight characters and graph still updates.
- Check serial event pipeline, SD errors, stability during BLE scanning and reconnection.

**Known limits:** The opt-in detail stream sends abbreviated nearby SSIDs/BLE advertised names over currently unauthenticated GATT. Avoid exposing sensitive identifying data, and use in trusted testing environments until bonded and authenticated. The BLE queue is bounded and details can be dropped; this is a survey sample, not a comprehensive inventory. No local firmware or Android compilation/hardware testing was executed by the assistant.
