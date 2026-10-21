#!/usr/bin/env python3
"""Offline, standard-library-only Callingcard JSONL analyzer."""
import argparse
import csv
import json
import sys
from collections import Counter
from pathlib import Path

SOURCES = ("wifi", "ble", "lora")
CSV_COLUMNS = ("file", "seq", "source", "uptime_ms", "utc", "lat", "lon",
               "rssi_dbm", "snr_db", "channel", "label")


def read_observations(paths, strict=False):
    """Yield valid observation dictionaries; never require full files in memory."""
    for path in paths:
        with path.open("r", encoding="utf-8") as handle:
            for lineno, line in enumerate(handle, 1):
                if not line.strip():
                    continue
                try:
                    obj = json.loads(line)
                except (ValueError, UnicodeError) as exc:
                    if strict:
                        raise ValueError(f"{path}:{lineno}: {exc}") from exc
                    print(f"warning: skipped malformed JSON {path}:{lineno}", file=sys.stderr)
                    continue
                if not isinstance(obj, dict) or obj.get("schema") != "callingcard.event.v0" or obj.get("kind") != "observation" or obj.get("source") not in SOURCES:
                    if strict:
                        raise ValueError(f"{path}:{lineno}: unsupported observation")
                    print(f"warning: skipped unsupported record {path}:{lineno}", file=sys.stderr)
                    continue
                yield path, obj


def coordinates(obj):
    pos = obj.get("position")
    if not isinstance(pos, dict):
        return None
    lat, lon = pos.get("lat"), pos.get("lon")
    if isinstance(lat, bool) or isinstance(lon, bool):
        return None
    if not isinstance(lat, (int, float)) or not isinstance(lon, (int, float)):
        return None
    if not (-90 <= lat <= 90 and -180 <= lon <= 180):
        return None
    return lat, lon


def summarize(paths, strict=False):
    per_file = {}
    for path, obj in read_observations(paths, strict):
        key = str(path)
        entry = per_file.setdefault(key, {"file": key, "observations": 0,
            "by_source": dict.fromkeys(SOURCES, 0), "with_utc": 0,
            "with_position": 0, "rssi_range_dbm": {},
            "first_utc": None, "last_utc": None})
        source = obj["source"]
        entry["observations"] += 1
        entry["by_source"][source] += 1
        utc = obj.get("utc")
        if isinstance(utc, str) and utc:
            entry["with_utc"] += 1
            entry["first_utc"] = min(entry["first_utc"], utc) if entry["first_utc"] else utc
            entry["last_utc"] = max(entry["last_utc"], utc) if entry["last_utc"] else utc
        if coordinates(obj):
            entry["with_position"] += 1
        rssi = obj.get("rssi_dbm")
        if isinstance(rssi, (int, float)) and not isinstance(rssi, bool):
            region = entry["rssi_range_dbm"].setdefault(source, {"min": rssi, "max": rssi})
            region["min"] = min(region["min"], rssi)
            region["max"] = max(region["max"], rssi)
    return [per_file.get(str(p), {"file": str(p), "observations": 0,
        "by_source": dict.fromkeys(SOURCES, 0), "with_utc": 0,
        "with_position": 0, "rssi_range_dbm": {}, "first_utc": None,
        "last_utc": None}) for p in paths]


def export_csv(paths, output, strict=False):
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_COLUMNS)
        writer.writeheader()
        for path, obj in read_observations(paths, strict):
            point = coordinates(obj)
            writer.writerow({"file": path.name, "seq": obj.get("seq"),
                "source": obj["source"], "uptime_ms": obj.get("uptime_ms"),
                "utc": obj.get("utc"), "lat": point[0] if point else None,
                "lon": point[1] if point else None, "rssi_dbm": obj.get("rssi_dbm"),
                "snr_db": obj.get("snr_db"), "channel": obj.get("channel"),
                "label": obj.get("label")})


def export_geojson(paths, output, strict=False):
    # Stream features without assembling a large in-memory collection.
    count = 0
    with output.open("w", encoding="utf-8") as handle:
        handle.write('{"type":"FeatureCollection","features":[\n')
        for path, obj in read_observations(paths, strict):
            point = coordinates(obj)
            if point is None:
                continue
            feature = {"type": "Feature",
                       "geometry": {"type": "Point", "coordinates": [point[1], point[0]]},
                       "properties": {"file": path.name, "seq": obj.get("seq"),
                           "source": obj["source"], "utc": obj.get("utc"),
                           "uptime_ms": obj.get("uptime_ms"), "rssi_dbm": obj.get("rssi_dbm"),
                           "snr_db": obj.get("snr_db"), "channel": obj.get("channel"),
                           "label": obj.get("label")}}
            if count:
                handle.write(",\n")
            json.dump(feature, handle, ensure_ascii=False, allow_nan=False)
            count += 1
        handle.write('\n]}\n')
    return count


def main(argv=None):
    parser = argparse.ArgumentParser(description="Offline Callingcard session analyzer (Python 3.9+)")
    parser.add_argument("inputs", nargs="+", type=Path, help="One or more Sxxx.JSONL files")
    parser.add_argument("--strict", action="store_true", help="Fail on malformed or unknown records")
    parser.add_argument("--csv", type=Path, help="Write per-observation CSV")
    parser.add_argument("--geojson", type=Path, help="Write positioned observations as GeoJSON")
    parser.add_argument("--summary-json", type=Path, help="Write summary as JSON")
    args = parser.parse_args(argv)
    for path in args.inputs:
        if not path.is_file():
            parser.error(f"not a file: {path}")
    try:
        summary = summarize(args.inputs, args.strict)
        print(json.dumps(summary, indent=2))
        if args.summary_json:
            args.summary_json.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        if args.csv:
            export_csv(args.inputs, args.csv, args.strict)
        if args.geojson:
            print(f"GeoJSON positioned features: {export_geojson(args.inputs, args.geojson, args.strict)}",
                  file=sys.stderr)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"error: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
