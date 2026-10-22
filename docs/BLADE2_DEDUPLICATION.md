# Blade 2 repeated scanner rows — PR 37

The focused Wi-Fi and BLE lists previously appended every sighting, so rediscovered networks and BLE advertisers filled the visible rows with duplicates.

## Behavior
- Wi-Fi: match by transmitted SSID plus channel; rediscovery moves the entry to the newest position, updates RSSI, and updates last-seen time.
- BLE: match by transmitted advertised name; rediscovery updates RSSI and most-recent ordering.
- Anonymous BLE advertisements and hidden Wi-Fi networks remain separate observations: their short wire format has no stable identifier to safely associate them.
- Different Wi-Fi channels with the same SSID intentionally stay separate, because they may be different radios/APs.
- The Wi-Fi channel-activity histogram retains a separate bounded set of raw observations over 120 seconds (up to 256 retained samples); deduplicating list rows does not erase the sample history.
- LoRa RX remains a chronological event list, not a deduplicated device list.

**Limitation:** The detail protocol transmits at most 23 bytes of a label and no BSSID or BLE MAC. Name-based matching cannot distinguish two BLE devices with the same advertised name, and labels truncated to identical prefixes may collapse unrelated devices. Future authenticated detail transport could carry stable pseudonymous identifiers.

## Test
Merge and pull main; rebuild/reinstall the `blade2/` APK in Android Studio. **No Cardputer firmware reflash required.** Confirm one Wi-Fi row per visible SSID/channel and one BLE row per advertised name across repeated surveys. Check RSSI is refreshed, Wi-Fi channel bars keep updating, swipe navigation and telemetry alerts work. Expect repeat rows for `<hidden>`/`<unnamed BLE>` where identity cannot be established.

Not compiled or tested on-device by assistant.
