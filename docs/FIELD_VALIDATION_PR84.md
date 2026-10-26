# PR84: Field performance and reliability validation

This is an **instrumentation/analysis milestone**, not a claim that radio reliability or detection accuracy has been validated. No BLE protocol, Wi-Fi scanning, GPS data path or Arduino/Android code changes.

## Repeatable trials
1. Merge PR84 and build `pio run -e cardputer-blade-wifi-integrated`. Record exact firmware git commit, Cardputer hardware/cap, GPS antenna placement, battery/power supply, Android APK version, and environmental conditions.
2. Collect at least **10 minutes** of uninterrupted USB serial logs for initial smoke testing, followed by **one hour** of observation for endurance. A host terminal can log `pio device monitor -e cardputer-blade-wifi-integrated | tee field-trial.txt` (note monitor may emit ANSI or carriage returns). Keep the recording session's SD JSONL file, too.
3. With the Blade connected, check `CYCLE mode=wifi`, `PASSIVE window`, `CYCLE mode=ble`, `BLADE connection_changed`, `BLADE connected=1 notify`, `PIPE`, and heap diagnostics. Note BLE survey intentionally skips when disconnected or in cooldown.
4. Run:
```sh
python3 tools/analyze_field_log.py field-trial.txt --output field-health.json
python3 -m unittest discover -s tests -p 'test_field_log_analysis.py'
```
5. Repeat with `cardputer-blade-evidence-v3` as an A/B comparison and document test conditions. Confirm GPS UTC and position validity with an independent reference. Inspect candidate detections against labeled fixtures before making false-positive claims.
6. If passive windows affect notification reliability or battery unexpectedly, roll back to `pio run -e cardputer-blade-evidence-v3 -t upload`.

## Collected metrics

- Active Wi-Fi and BLE completed cycles, passive windows and maximum observed scan durations
- Scan timeouts, passive setup/channel failures
- Blade connection/disconnection events and the **highest reported notification-attempt count**, which **is not delivery confirmation**
- BLE prequeue drops, pipeline drops, minimum observed free heap
- Candidate log-line count, **not** true-positive or false-positive rate

Absent log fields become `null` rather than fabricated measurements. Cumulative drop counters may reset after reboot; the analyzer reports their **largest observed values**, not summed totals. Counts assume reasonably complete USB logs. For production validation, add frame acknowledgements or receiver-side telemetry and event clocks to estimate notification loss and latency; those are *not* inferred from attempts alone.

## Review and acceptance

Record an external test sheet with: environment name, commit, date/duration, battery/power, location conditions, Blade model/app version, start/end connection state, observed disconnects, scan failures, minimum heap, GPS fix quality, hand-labeled signature precision, and known regressions.

**Provisional go/no-go guidance:** no unexpected reset, bounded heap without continual decline, no repeatable scan starvation, stable Blade reconnection, and no spurious high-confidence device claims. These are qualitative gates, not guaranteed reliability thresholds. Do not interpret receiver GNSS as target location.

All test results must come from real runs. This PR contains the analysis tool and automated parser fixtures only; field measurements are pending.
