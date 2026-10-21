# PR 27 — Offline Mac survey analysis

Host-side Python 3.9+ standard-library-only tool. It works offline with Callingcard event.v0 JSONL files. No firmware changes.

## Usage

Copy closed recordings from microSD /CALLINGCARD/Sxxx.JSONL to your Mac. From the repo root:

    python3 tools/analyze_sessions.py ~/Callingcard/S001.JSONL

Multiple sessions with exports:

    python3 tools/analyze_sessions.py ~/Callingcard/S001.JSONL ~/Callingcard/S002.JSONL --csv observations.csv --geojson positioned.geojson --summary-json summary.json

Tests:

    python3 -m unittest discover -s tests -p 'test_analyze_sessions.py' -v

## Outputs

- Terminal / summary.json: per-file counts for Wi-Fi/BLE/LoRa, UTC/position coverage, RSSI minimum/maximum, first and last UTC.
- observations.csv: all valid observations, including those lacking a GNSS fix (blank lat/lon).
- positioned.geojson: GeoJSON Point features for only located observations; [longitude, latitude] ordering.

Warnings for bad JSON/unsupported entries by default; --strict fails instead. The script streams input/GeoJSON rather than loading entire files and never modifies inputs. Existing output files are overwritten.

## Privacy and accuracy

GeoJSON/CSV may expose precise GPS coordinates, SSIDs, BLE advertised names. Treat these as sensitive and redact before sharing. The script makes no network requests. Results are observation events, not distinct physical devices or a complete picture of all RF traffic. GeoJSON excludes null-position events while CSV retains them.

## Validation

Run the tests on macOS; inspect actual completed Sxxx.JSONL sessions. Verify CSV totals match JSONL counts and GeoJSON contains only valid-position records, including a file with missing GPS and a deliberately malformed last line.

Status: code committed but not run on a Mac or tested against actual user logs by the assistant.
