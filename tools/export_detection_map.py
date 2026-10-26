#!/usr/bin/env python3
"""Export Callingcard observation JSONL detections to CSV + GeoJSON.

Only records with a detection and a valid numeric position are geolocated.
No guessed positions; all candidates remain unverified classifications.
"""
import argparse
import csv
import json
import math
from pathlib import Path

FIELDS = ["utc", "source", "type", "confidence", "rule", "identifier",
          "label", "rssi_dbm", "channel", "latitude", "longitude"]

def extract(path):
    with Path(path).open(encoding="utf-8") as stream:
        for line_no, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                item = json.loads(line)
            except json.JSONDecodeError:
                continue  # SD data can be interrupted mid-record.
            detection = item.get("detection")
            if not isinstance(detection, dict):
                continue
            source = item.get("source")
            if source not in ("wifi", "ble"):
                continue
            pos = item.get("position")
            lat = lon = None
            if isinstance(pos, dict):
                x, y = pos.get("lat"), pos.get("lon")
                if (type(x) in (float, int) and type(y) in (float, int)
                    and math.isfinite(x) and math.isfinite(y)
                    and -90 <= x <= 90 and -180 <= y <= 180):
                    lat, lon = x, y
            yield {
                "utc": item.get("utc") or "",
                "source": source,
                "type": detection.get("type", ""),
                "confidence": detection.get("confidence", ""),
                "rule": detection.get("rule", ""),
                "identifier": item.get("bssid", "") if source == "wifi"
                              else item.get("ble_address", ""),
                "label": item.get("label", ""),
                "rssi_dbm": item.get("rssi_dbm", ""),
                "channel": item.get("channel", ""),
                "latitude": lat, "longitude": lon,
            }

def export(input_path, csv_path, geojson_path):
    rows = list(extract(input_path))
    with Path(csv_path).open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: "" if v is None else v for k, v in row.items()})
    features = []
    for row in rows:
        if row["latitude"] is None:
            continue
        props = {k: v for k, v in row.items() if k not in ("latitude", "longitude")}
        features.append({"type": "Feature",
                         "geometry": {"type": "Point",
                                      "coordinates": [row["longitude"], row["latitude"]]},
                         "properties": props})
    with Path(geojson_path).open("w", encoding="utf-8") as out:
        json.dump({"type": "FeatureCollection", "features": features}, out,
                  indent=2, ensure_ascii=False)
        out.write("\n")
    return len(rows), len(features)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jsonl", help="Callingcard session JSONL file")
    parser.add_argument("--csv", default="detections.csv")
    parser.add_argument("--geojson", default="detections.geojson")
    args = parser.parse_args()
    records, located = export(args.jsonl, args.csv, args.geojson)
    print(f"Exported {records} detection candidates ({located} geolocated).")
