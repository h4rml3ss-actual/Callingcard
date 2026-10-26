import importlib.util
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location("health",Path(__file__).resolve().parents[1]/"tools/analyze_field_log.py")
health=importlib.util.module_from_spec(spec);spec.loader.exec_module(health)
class HealthTests(unittest.TestCase):
    def test_mixed_logs(self):
        lines=[
            "BLADE connection_changed=1",
            "CYCLE mode=wifi count=8 duration_ms=2120 timeouts=0 heap=102400 errors=0",
            "PASSIVE window=1 beacon=9 probe_req=4 heap=99800",
            "CYCLE mode=ble unique=5 callbacks=5 duration_ms=3050 prequeue_dropped=2 timeouts=0 heap=95000 errors=0",
            "BLADE connected=1 notify=1,8,5,0,1,1 attempts=7",
            "CYCLE mode=wifi timeout=1",
            "CYCLE mode=ble timeout=1",
            "PASSIVE start_failed=1",
            "PIPE queue=0 emitted=100 dropped=4 heap=94000",
            "DETECT candidate=FLOCK confidence=LOW rule=test",
            "BLADE connection_changed=0",
        ]
        data=health.analyze(lines)
        self.assertEqual(data["wifi_cycles"],1)
        self.assertEqual(data["ble_cycles"],1)
        self.assertEqual(data["passive_windows"],1)
        self.assertEqual(data["blade_disconnects"],1)
        self.assertEqual(data["minimum_heap_bytes"],94000)
        self.assertEqual(data["ble_prequeue_dropped_max"],2)
        self.assertEqual(data["pipeline_dropped_max"],4)
        self.assertEqual(data["wifi_duration_ms_max"],2120)
        self.assertEqual(data["blade_notification_attempts_max"],7)
        self.assertEqual(data["false_positive_rate"][:12],"not measured")
    def test_empty_and_missing(self):
        data=health.analyze(["unrecognized line","CYCLE mode=wifi count=1"])
        self.assertEqual(data["wifi_cycles"],1)
        self.assertIsNone(data["minimum_heap_bytes"])
        self.assertIsNone(data["wifi_duration_ms_max"])
        self.assertEqual(data["blade_disconnects"],0)
if __name__=="__main__":unittest.main()
