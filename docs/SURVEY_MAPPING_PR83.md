# PR83 — Survey mapping exports and summaries

Extends PR76's offline exporter without altering Cardputer firmware or Blade Android.

## Commands
```sh
python3 tools/export_detection_map.py S000.JSONL --csv detections.csv --geojson detections.geojson --summary summary.json
python3 tools/export_detection_map.py S000.JSONL --csv flock.csv --geojson flock.geojson --summary flock-summary.json --type FLOCK --source wifi --confidence LOW --deduplicate
python3 -m unittest discover -s tests -p 'test_export_detection_map.py'
```

- CSV continues to include **all filtered classified observations**, whether or not GNSS is available.
- Standard GeoJSON continues to include every located observation.
- `--deduplicate` changes only GeoJSON: for each exact `source + detection type + advertised address`, select the *strongest located receiver observation*. Unidentified observations remain separate. It does **not** calculate or claim a transmitter location.
- `--summary` writes JSON containing counts, categories, source/confidence distribution, located/missing positions, mapped point count, first/last available UTC, and distinct address/category keys. Each input file is treated as a survey; counts are not merged across files.
- The deduplication key is not stable physical identity. BLE advertiser addresses may rotate, be reused or be spoofed. Repeated AP BSSID readings also do not establish a camera's whereabouts.
- PR82 boot-scoped `correlation` annotations remain available in raw JSONL; the exporter uses explicit identifier grouping rather than assuming that a cached sighting count is a session-specific count.
- Missing/invalid coordinates never become map points. GeoJSON order is longitude, latitude.
- Classification continues to mean **candidate**, not confirmed detection.

## Validation
Run the unit tests above. Compare standard versus `--deduplicate` maps from a controlled recording; verify CSV remains uncollapsed. Check that two separate locations associated with the same BSSID result in one representative point in the deduplicated map, without implying that point is the actual transmitter location. Android APK and Cardputer firmware do not require changes.

Build/tests not independently executed; verify before field use.
