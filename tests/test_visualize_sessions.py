import json
import tempfile
import unittest
from pathlib import Path
from tools.visualize_sessions import build_records, render_html


class ViewerTests(unittest.TestCase):
    def test_offline_report_and_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "S001.JSONL"
            row = {"schema": "callingcard.event.v0", "kind": "observation",
                   "source": "wifi", "uptime_ms": 12000, "rssi_dbm": -51,
                   "label": "</script><img src=x onerror=alert(1)>",
                   "utc": "2026-10-22T12:00:00Z",
                   "position": {"lat": 42.3, "lon": -71.1}}
            path.write_text(json.dumps(row)+"\n",encoding="utf-8")
            rows,total,stride=build_records([path])
            self.assertEqual((len(rows),total,stride),(1,1,1))
            self.assertEqual((rows[0]["lat"],rows[0]["lon"]),(42.3,-71.1))
            output=render_html(rows)
            self.assertNotIn("</script><img",output)
            self.assertIn("\\u003c",output)
            self.assertIn("default-src 'none'",output)
            self.assertIn('id="timeline"',output)

    def test_bounded_records_and_null_coordinates(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"S002.JSONL"
            row={"schema":"callingcard.event.v0","kind":"observation",
                 "source":"ble","uptime_ms":100,"rssi_dbm":-80,
                 "position":None,"utc":None,"label":"example"}
            path.write_text((json.dumps(row)+"\n")*11,encoding="utf-8")
            rows,total,stride=build_records([path],limit=3)
            self.assertEqual(total,11)
            self.assertLessEqual(len(rows),3)
            self.assertIsNone(rows[0]["lat"])
            self.assertEqual(stride,4)


if __name__ == "__main__":
    unittest.main()
