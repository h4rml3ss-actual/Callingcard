#!/usr/bin/env python3
"""Export Callingcard observation JSONL detections to CSV + GeoJSON.

Only records with a detection and a valid numeric position are geolocated.
No guessed positions; all candidates remain unverified classifications.
"""
import argparse
import csv
import json
import math
from collections import Counter
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

def export(input_path, csv_path, geojson_path, *, category=None, source=None, confidence=None,\n           deduplicate=False, summary_path=None):
    rows = [row for row in extract(input_path)
            if (category is None or row["type"] == category)
            and (source is None or row["source"] == source)
            and (confidence is None or row["confidence"] == confidence)]
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
    if deduplicate:
        # One representative *observation* per source/type/address.
        # Choose the strongest located reading, never average coordinates
        # (receiver positions are not transmitter locations).
        grouped = {}
        unkeyed = []
        for row in rows:
            if row["latitude"] is None:
                continue
            if not row["identifier"]:
                unkeyed.append(row)
                continue
            key = (row["source"], row["type"], row["identifier"].lower())
            previous = grouped.get(key)
            strength = row["rssi_dbm"] if type(row["rssi_dbm"]) in (int, float) else -999
            prev_strength = (previous["rssi_dbm"] if previous and
                type(previous["rssi_dbm"]) in (int, float) else -999)
            if previous is None or strength > prev_strength:
                grouped[key] = row
        selected = list(grouped.values()) + unkeyed
        features = []
        for row in selected:
            props = {k: v for k, v in row.items() if k not in ("latitude", "longitude")}
            features.append({"type": "Feature",
                             "geometry": {"type": "Point", "coordinates":
                                          [row["longitude"], row["latitude"]]},
                             "properties": props})
    if summary_path is not None:
        with Path(summary_path).open("w", encoding="utf-8") as out:
            json.dump({
                "schema": "callingcard.survey-summary.v1",
                "input_file": Path(input_path).name,
                "candidate_observations": len(rows),
                "geolocated_observations": sum(r["latitude"] is not None for r in rows),
                "mapped_points": len(features),
                "missing_positions": sum(r["latitude"] is None for r in rows),
                "categories": dict(sorted(Counter(r["type"] for r in rows).items())),
                "sources": dict(sorted(Counter(r["source"] for r in rows).items())),
                "confidences": dict(sorted(Counter(r["confidence"] for r in rows).items())),
                "distinct_address_category_keys": len({
                    (r["source"],r["type"],r["identifier"].lower())
                    for r in rows if r["identifier"]}),
                "first_utc": min((r["utc"] for r in rows if r["utc"]), default=None),
                "last_utc": max((r["utc"] for r in rows if r["utc"]), default=None),
            }, out, indent=2, ensure_ascii=False)
            out.write("\\n")
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
    parser.add_argument("--summary", help="Optional JSON survey summary output")
    parser.add_argument("--type", dest="category", help="Filter candidate category (e.g. FLOCK)")
    parser.add_argument("--source", choices=["wifi", "ble"], help="Filter radio source")
    parser.add_argument("--confidence", choices=["LOW", "MEDIUM", "HIGH"])
    parser.add_argument("--deduplicate", action="store_true",
                        help="Map one strongest receiver observation per exact source/type/address")
    args = parser.parse_args()
    records, located = export(args.jsonl, args.csv, args.geojson,
        category=args.category, source=args.source, confidence=args.confidence,
        deduplicate=args.deduplicate, summary_path=args.summary)
    print(f"Exported {records} detection candidates ({located} geolocated).")
