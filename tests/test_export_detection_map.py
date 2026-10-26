import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("exporter", ROOT / "tools/export_detection_map.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ExportTest(unittest.TestCase):
    def test_geolocation_and_missing_fix(self):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            source = folder / "survey.jsonl"
            events = [
                {"source":"wifi","label":"test","bssid":"B4:1E:52:00:00:01",
                 "detection":{"type":"FLOCK","confidence":"MEDIUM","rule":"test"},
                 "position":{"lat":42.3,"lon":-71.1},"utc":"2026-10-26T10:00:00Z"},
                {"source":"ble","label":"Tile Pro","ble_address":"AA:BB:CC:DD:EE:FF",
                 "detection":{"type":"TRACKER","confidence":"LOW","rule":"test2"},
                 "position":None,"utc":None},
                {"source":"wifi","label":"generic","position":{"lat":42,"lon":-71}},
                {"source":"ble","detection":{"type":"TRACKER"},
                 "position":{"lat":float("nan"),"lon":9}}
            ]
            source.write_text("\n".join(json.dumps(x) for x in events)+"\n" + "{bad json\n")
            count,located=module.export(source,folder/"map.csv",folder/"map.geojson")
            self.assertEqual((count,located),(3,1))
            map_data=json.loads((folder/"map.geojson").read_text())
            self.assertEqual(map_data["features"][0]["geometry"]["coordinates"],[-71.1,42.3])
            self.assertEqual(map_data["features"][0]["properties"]["identifier"],"B4:1E:52:00:00:01")
            csv_data=(folder/"map.csv").read_text()
            self.assertIn("AA:BB:CC:DD:EE:FF",csv_data)

if __name__=="__main__":
    unittest.main()
