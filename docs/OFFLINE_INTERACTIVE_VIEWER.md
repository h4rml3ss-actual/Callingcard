# PR 28 — Offline interactive survey viewer

This is a macOS-friendly, **no-dependency** Python 3.9+ report generator. It uses the same Callingcard JSONL input parser as PR #27. It generates a **self-contained HTML file** with built-in JavaScript and SVG; it does not fetch maps, JavaScript libraries, fonts, tiles, or data.

## How to use

From the Callingcard repository root, after copying closed microSD sessions to the Mac:

    python3 tools/visualize_sessions.py ~/Callingcard/S001.JSONL ~/Callingcard/S002.JSONL --output callingcard-viewer.html

Open callingcard-viewer.html locally in Safari, Firefox, or Chrome.

Run tests:

    python3 -m unittest discover -s tests -p 'test_*.py' -v

## Features

- Filter by session and radio (Wi-Fi, BLE, LoRa).
- Toggle 5, 15, or 60-second observation-count timeline bins.
- Explore RSSI scatterplots (uptime vs dBm).
- Explore location-tagged observations in a **relative longitude/latitude scatterplot**. This is deliberately not a street map; no map services, downloads, or geocoding are used.
- Hover over points/bars for details. Missing coordinates are excluded only from the position plot.
- Handles multiple historical JSONL files and warns about unsupported/malformed lines. Use --strict to fail on them.

## Limits and accuracy

- A deterministic global downsampling cap of 12,000 observations avoids enormous offline HTML reports. Printed totals describe included samples only, not the entire input when downsampling occurs. The CLI reports processed records, included records, and stride. The sampling policy is simple global stride; it may undersample rare LoRa events or short sessions when datasets are large.
- Source timestamps are device **uptime milliseconds**. Separate sessions start at different boot times; select one session for a meaningful timeline. Wi-Fi survey entries may cluster around scan completion.
- Position dots can overlap when readings are repeated. GPS accuracy is not evaluated. Signal strength is not a distance or coverage guarantee.
- Observation totals are not counts of unique wireless devices.
- The generated HTML contains SSIDs, advertised BLE names, and optionally precise coordinates. Keep it private unless intentionally redacted. No network access is required or performed by the viewer.
- Source log files are only read; outputs are saved to the requested --output path.

## Validation

Generate a report from both a GNSS-fixed and an indoor/no-fix session; verify the map plot contains only positioned events, charts respond to filters, timeline bars age consistently, and there are no network requests in the browser's developer tools. Verify the Python unit tests.

Development status: committed through GitHub; not yet tested with the user's recordings or browser.
