import unittest
from tools.visualize_sessions import render_html

class ViewerFiltersTests(unittest.TestCase):
    def test_filter_controls_and_offline_map_context(self):
        report=render_html([{"file":"S001.JSONL","source":"wifi","t":1000,"rssi":-60,
            "utc":None,"lat":42.3,"lon":-71.1,"label":"example"}])
        for token in ('id="search"', 'id="minRssi"', 'id="onlyLocated"',
                      'Coordinate bounds:', 'WGS84 degrees', 'id="bounds"',
                      "r.label.toLowerCase().includes(name)", "r.rssi>=minRssi"):
            self.assertIn(token, report)
        self.assertIn("default-src 'none'", report)
        self.assertNotIn('https://',report)
    def test_escaped_observation_name(self):
        report=render_html([{"file":"S001.JSONL","source":"ble","t":1000,"rssi":-52,
            "utc":None,"lat":None,"lon":None,"label":"</script><img src=x>"}])
        self.assertNotIn("</script><img",report)
        self.assertIn(r"\u003c",report)

if __name__=="__main__":
    unittest.main()
