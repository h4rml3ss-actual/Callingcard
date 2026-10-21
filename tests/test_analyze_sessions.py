import json
import tempfile
import unittest
from pathlib import Path
from tools.analyze_sessions import summarize, export_csv, export_geojson, coordinates, read_observations


def observation(source, position=None, utc=None, rssi=-62):
    return {"schema": "callingcard.event.v0", "kind": "observation", "seq": 1,
            "source": source, "uptime_ms": 1000, "rssi_dbm": rssi,
            "channel": None, "label": "example", "utc": utc, "position": position}


class AnalyzerTests(unittest.TestCase):
    def test_counts_and_geospatial_export(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "S001.JSONL"
            objects = [
                observation("wifi", {"lat": 42.3, "lon": -71.1}, "2026-10-21T17:32:03Z"),
                observation("ble"),
                observation("lora", {"lat": 99, "lon": 0}),
            ]
            source.write_text("".join(json.dumps(obj) + "\n" for obj in objects))
            summary = summarize([source])[0]
            self.assertEqual(summary["observations"], 3)
            self.assertEqual(summary["by_source"], {"wifi": 1, "ble": 1, "lora": 1})
            self.assertEqual(summary["with_position"], 1)
            self.assertEqual(summary["with_utc"], 1)
            csv_path, geo_path = root / "out.csv", root / "out.geojson"
            export_csv([source], csv_path)
            self.assertEqual(len(csv_path.read_text().splitlines()), 4)
            self.assertEqual(export_geojson([source], geo_path), 1)
            geo = json.loads(geo_path.read_text())
            self.assertEqual(geo["features"][0]["geometry"]["coordinates"], [-71.1, 42.3])

    def test_malformed_and_strict(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / "S002.JSONL"
            file.write_text("{bad json}\n" + json.dumps(observation("wifi")) + "\n")
            self.assertEqual(summarize([file])[0]["observations"], 1)
            with self.assertRaises(ValueError):
                summarize([file], strict=True)

    def test_invalid_positions_and_empty(self):
        self.assertIsNone(coordinates(observation("ble", {"lat": True, "lon": 10})))
        self.assertIsNone(coordinates(observation("ble", {"lat": 0, "lon": 190})))
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "S000.JSONL"
            p.touch()
            self.assertEqual(summarize([p])[0]["observations"], 0)


if __name__ == "__main__":
    unittest.main()
