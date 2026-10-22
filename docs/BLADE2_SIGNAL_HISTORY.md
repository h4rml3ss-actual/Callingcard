# Blade 2 signal history (PR #39)

## What changes
The Blade 2 selected-observation detail panel now plots up to 24 recent RSSI measurements using a fixed -110 to -20 dBm vertical scale. Each Wi-Fi row tracks recurring observations with the same received SSID **and channel**; each BLE row tracks recurring advertisements with the same transmitted name. The history stays on the glasses in RAM only; disconnect/restart clears it.

LoRa details now include the actual receive RSSI and an SNR field encoded into the existing detail label as `SNR:<tenths of a dB>` (example `SNR:-78` means -7.8 dB). Chronological LoRa rows preserve a shared receive-RSSI trend. No LoRa payload or frequency change; existing status/alert BLE characteristics remain unchanged.

## Caveats
- Existing details are still capped by the transport's 23-byte observation name; this PR does **not** add device IDs. BLE name matching can merge two physical advertisers with identical names; hidden Wi-Fi and unnamed BLE observations cannot be stably matched and have one-sample trends.
- LoRa RSSI/SNR are **receive metrics of the most recent successfully decoded packet**, not live spectrum measurements. No precise location, BSSID or BLE MAC is transmitted.
- Wi-Fi scans only forward a bounded sample of APs per survey, and BLE scanning is throttled by the coexistence profile, so graphs fill gradually.
- Current BLE GATT is unauthenticated. Short SSIDs/BLE names can be read by any nearby GATT client; use experimental detail profile only where appropriate.

## Install / validation
1. Merge PR; pull main. Rebuild `blade2/` APK in Android Studio and reinstall on Blade 2.
2. Flash Cardputer `pio run -e cardputer-blade-details -t upload` (this profile retains the tested coexistence settings). Open serial monitor if needed.
3. Swipe to Wi-Fi or BLE, select a named item and observe several repeat scans. Verify the detail RSSI graph gets multiple samples while the row remains deduplicated.
4. Receive LoRa traffic and inspect its detail view. Confirm RSSI is nonzero and SNR shows tenths-scaled dB, with graph history across packets.
5. Verify HUD LIVE/STALE, notifications, scroll/selection, SD errors and reconnect stability. If regressions occur, restore previously validated firmware and report log output.

No on-device compile or hardware testing was performed by assistant.
