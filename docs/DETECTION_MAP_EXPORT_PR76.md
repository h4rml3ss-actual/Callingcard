# PR76 — Detection identifiers and map-ready export

Adds observation-time Wi-Fi BSSID and BLE advertiser address snapshots to Cardputer JSONL event records, and a local Python 3 exporter for mapping and field-survey review. No changes to Cardputer-to-Blade BLE GATT messages, Android app, scan timing, or GPS freshness rules.

## Capture and interpretation
- Wi-Fi events include `bssid`, if available.
- BLE events include `ble_address`, if available. BLE addresses may be random or change frequently; an address is not definitive physical device identity.
- Existing `detection` (candidate type/confidence/rule), `position`, `utc`, `label`, and radio strength remain unchanged.
- JSONL records still contain an optional `position`, populated only from the existing fresh GNSS snapshot; no retroactive location guesses.
- The collector does not store raw BLE advertisements or passive Wi-Fi packet payloads.

## Export

```bash
python3 tools/export_detection_map.py /path/to/S000.JSONL \
  --csv detections.csv --geojson detections.geojson
python3 -m unittest discover -s tests -p 'test_export_detection_map.py'
pio run -e cardputer-blade-details
```

The CSV includes all classified Wi-Fi/BLE detections, including those without a GPS fix (empty coordinates). The GeoJSON contains **only** records with valid latitude and longitude, in GeoJSON's correct longitude/latitude coordinate order. It works with QGIS, geojson.io, and other geospatial tools. Partial/malformed JSONL lines are skipped.

## Important limits
- These are **possible detections**, not confirmed ALPR/Flock/tracker identification; generic/shared vendor signatures remain weak evidence.
- At present, the Blade still displays candidate/coordinates from PR74, and **does not yet receive BSSID or BLE addresses**. Identifier-plus-evidence BLE fragments need an explicit versioned protocol, MTU testing and reliable reassembly before enabling. This is a narrowly scoped first step for trustworthy export and device-correlation experiments.
- Existing 512-byte JSONL record cap and SD reliability are still worth stress testing.
- Data includes wireless identifiers and possibly coordinates: the resulting files should be handled as field-survey records.
- Compilation and physical hardware testing remain pending.
