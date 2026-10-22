# PR 29 — Offline viewer observation filters and coordinate context

Adds interactive filtering to the self-contained Mac HTML viewer introduced in PR #28. No Cardputer firmware changes.

## New controls

- **Name contains:** Case-insensitive substring filter for SSIDs and BLE advertised names. This is a text search, not a physical device identity filter.
- **Minimum RSSI:** Include only observations whose RSSI is at least the entered dBm threshold (e.g. -70). Blank includes all. Rows with missing RSSI are excluded if a threshold is active.
- **GNSS fix only:** Include only observations with valid latitude and longitude from the analyzer.
- Session and Wi-Fi/BLE/LoRa checkboxes continue to work and all charts and aggregate counts update together.

## Geographic context

The existing offline coordinate scatterplot now has a light reference grid and exact minimum/maximum latitude and longitude labels for the selected records, in WGS84 degrees. This remains **a relative coordinate plot, not a geographically scaled street map or range/triangulation estimate**. Grid spacing in the display is not a promise of equal physical distances, especially at differing latitudes. No tile servers, browser location access, network calls, or cloud dependencies.

## Workflow

Generate as in PR #28:

    python3 tools/visualize_sessions.py ~/Callingcard/S001.JSONL --output callingcard-viewer.html

Open the file locally in a browser and try a label fragment, RSSI >= -70, and GNSS-only. Observe the graph, scatterplot, counts, and coordinate bounds change together.

Tests:

    python3 -m unittest discover -s tests -p 'test_*.py' -v

## Privacy and sampling

The HTML still embeds local observation data, including identifiers and potentially sensitive GPS coordinates. Do not share without reviewing it. The 12,000-point deterministic downsampling from PR #28 remains; all filters operate on **sampled records**, so rare observations may have been excluded before filtering. For complete exports use the PR #27 CSV/GeoJSON analyzer.

Code committed to GitHub, but not yet run on the user's Mac/browser.
