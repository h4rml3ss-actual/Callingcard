# Blade 2 graphical scanner pages — experimental PR 35

## Design and controls

Blade 2 is a 480x480 single-eye display. Its touchpad in gesture mode provides DPAD left/right fallback key events on swipes; the Android app intercepts them to rotate:
**Overview → Wi-Fi → BLE → LoRa → Overview.** Use **Settings > Device > Touchpad > Gesture** (not Mouse mode).

Android renders with a Canvas view (no new dependencies). Wi-Fi shows **observed 2.4 GHz channel sample counts** over the last 120 seconds as 14 vertical bars. This is **not** an RF spectrum analyzer and does not measure utilization/noise/interference; a channel bar is the number of detail observations delivered to the glasses, not all APs in the area. The current firmware only queues up to five Wi-Fi observations per scan, so the plot is intentionally a sample.

Focused pages show the most recent short SSIDs/BLE advertised names and RSSI values, and LoRa reception entries (not packet contents). The total list is bounded to 32 per source in Android. The firmware sends a bounded queue of 12 observation summaries, one at most every 250ms. Further observations are dropped from **HUD detail streaming only** when the queue is full; the existing serial/SD event pipeline is independent. Names are truncated to eight bytes and may cut multibyte Unicode. BLE device identity/address and GPS coordinates are never sent. LoRa detailed RSSI is not yet available from the existing CapSensors accessor; those rows simply say RX.

## BLE detail wire format

Optional characteristic `cabb1e00-ccad-4abc-a112-24fc00000004` READ/NOTIFY, payload `D,<W|B|L>,<channel>,<rssi>,<label>`. Each message fits within the default BLE MTU payload (19 chars max). Status and event alert characteristic UUIDs/payloads remain unchanged. The Blade app gracefully handles firmware without details.

**Privacy:** The detail endpoint broadcasts nearby SSIDs and BLE advertised names via an unauthenticated GATT service to any nearby GATT client that connects. Use this opt-in diagnostic build in trusted environments only. Production use requires authentication, bonding and opt-in data minimization before extending details further.

## Test

1. Merge, pull main. In Android Studio open `blade2/`, rebuild/reinstall on Blade 2.
2. Upload experimental firmware:
   ```sh
   pio run -e cardputer-blade-details
   pio run -e cardputer-blade-details -t upload
   pio device monitor -e cardputer-blade-details
   ```
3. Launch the HUD and verify overview counters, alerts and LIVE/STALE behavior from PR33/34.
4. Swipe left/right using Blade touchpad in Gesture mode; check pages. Check the Wi-Fi histogram starts filling after Wi-Fi surveys and short SSIDs appear, BLE names after its ~30s survey intervals, and LoRa receives after packet activity.
5. Verify BLE connectivity during survey windows; ensure SD errors remain zero and serial log performance stable. Report display clipping, swipe orientation and scan throughput.
6. Restore previously proven `cardputer-blade-alerts` or `cardputer-blade-coexist` firmware if regression occurs. Existing Android app can still display basic status/alerts against those profiles.

This PR is uncompiled and untested by assistant on a physical Blade 2 or Cardputer. The histogram is a **2.4 GHz observation distribution**, not a spectrum analyzer. 5 GHz detail entries have no bar yet.
