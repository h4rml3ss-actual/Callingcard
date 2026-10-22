# Blade 2 interactive scanner details (PR #38)

This is an Android-only upgrade. The tested `cardputer-blade-details` firmware and GATT protocol remain unchanged.

## Controls

- **Swipe left/right** (Android DPAD_LEFT / DPAD_RIGHT): change Overview → Wi-Fi → BLE → LoRa scanner page. Does nothing while the detail panel is open.
- **Swipe up/down** (DPAD_UP / DPAD_DOWN): move the highlighted selection through the currently received scanner list. The visible Wi-Fi list has three rows; BLE and LoRa lists have six, and selecting past the edge scrolls the window.
- **Touchpad select / center** (DPAD_CENTER or ENTER): open details for the selected row.
- **Back** (Android BACK): return to scanner list; Back from a scanner page falls back to Android behavior.

## Details / limitations

Shows source, the full label **as received by the Blade app**, Wi-Fi channel when known, RSSI for Wi-Fi/BLE, and elapsed time since last sighting. The current firmware limits detail labels to up to 23 bytes, and does not include BSSID, BLE identity, device manufacturer, LoRa RSSI/SNR, or other rich metadata. This view **cannot reconstruct** a label cut off in BLE transport.

Wi-Fi entries are still deduplicated by SSID/channel and BLE entries by name. Active selection follows a static row index while the list is open, so arriving/newly reordered scan observations may change which row is highlighted; an open detail page holds its chosen observation and refreshes values on matching new sightings. Future iteration can anchor selection to stable identifiers after authentication is added.

## Validation

1. Merge PR, pull latest main, open `blade2/` in Android Studio, build and install the debug APK. **Do not reflash the Cardputer**.
2. Swipe left/right and confirm scanner pages still work.
3. On Wi-Fi/BLE pages, swipe down several times and check the highlighted row moves; after the visible edge, older entries should scroll into view. Swipe up to return.
4. Press touchpad center to open a detail page; check the label, Wi-Fi channel, RSSI and Last Seen age. Press Back to return.
5. Confirm overview freshness, alerts, channel activity histogram and scanning continue, including reconnect.
6. If touchpad center or vertical gestures are not mapped to DPAD keys on your Blade firmware, capture Android input events and we will adapt the handlers.

This PR has not been compiled or hardware-validated by the assistant.
