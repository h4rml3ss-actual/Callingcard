# PR 32 — Blade 2 BLE scanning coexistence experiment

We have verified live GATT telemetry on the Blade 2 with PR #31's GATT-only profile. This PR introduces an **opt-in, connected-first coexistence experiment** rather than altering the known-working profile.

## Build and test

1. Keep the proven recovery environment available:

       pio run -e cardputer-blade-gatt-only -t upload

2. To try BLE discovery while maintaining the HUD link:

       pio run -e cardputer-blade-coexist
       pio run -e cardputer-blade-coexist -t upload
       pio device monitor -e cardputer-blade-coexist

3. On Blade 2, launch the existing HUD app (no Android project changes in PR #32), and establish a link **before** attempting BLE survey. Wi-Fi/LoRa/GNSS and logging continue while waiting.

4. Once connected, Cardputer allows **two-second passive BLE scan windows at least 30 seconds apart**. When disconnected, new BLE survey scans are held so advertising/discovery can resume. Status output includes BLADE COEX connected, ble_scans, skipped, dropped, prequeue and the earlier GATT notification log.

5. Keep the glasses running for 10–15 minutes; check the HUD receives updated Wi-Fi, BLE, LoRa, GNSS, and recording values, BLE survey observations appear in serial JSONL after connection, and notification activity continues. Walk briefly out of range and return to verify reconnect. Compare to GATT-only baseline and track scan timeouts, SD errors and event loss.

## Interpretation

- Working connection and new BLE observations: coexistence is promising, but still requires a longer soak test.
- Connection works, but new BLE scans drop the link: stop the test and revert to GATT-only; we'll need a different BLE stack/scheduling approach.
- Initial HUD discovery fails: investigate advertising state again; scan windows are not initiated until connected.
- This implementation does not assert Bluetooth stack scan + server concurrency is universally reliable.

The original cardputer-adv, cardputer-blade-poc and cardputer-blade-gatt-only environments remain intact.

**Security:** Telemetry is still unauthenticated/unpaired GATT with aggregate data only. Do not transmit identifiers, coordinates or control commands yet.

**Status:** GitHub changes have not been compiled or tested by the assistant. Validate both builds before field use.
