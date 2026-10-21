# PR 26 — RF observation activity dashboard

Press **A** to open/close the RF graph. **H** switches to the existing history browser. The activity graph is a rolling **120-second window** of successfully enqueued Wi-Fi, BLE and LoRa observations, grouped into 24 five-second time bins.

Three rows show per-source totals and normalized bar charts. Colors distinguish sources. Each row independently scales its highest count to the available chart height; bar heights **cannot be compared between radios as absolute volume**. Numbers at left are 120-second observation totals.

## Data semantics and limitations

- Wi-Fi survey events are created when the asynchronous survey completes; all discovered Wi-Fi observations may cluster in one bin rather than spread across the survey interval.
- BLE events use callback-time uptime; LoRa uses the timestamp assigned by the existing receive pipeline. Old observations arriving late can be binned by their captured uptime until their bucket expires.
- These are **observation counts, not spectral energy, RF power readings, unique devices, or packet-rate monitoring**. Unobserved BLE frames and LoRa payloads aren't counted.
- Only events successfully entering the EventPipeline count. Existing `prequeue_dropped` / `PIPE dropped` metrics continue to diagnose losses.
- Fixed 3 × 24 × 4-byte counter array; zero heap allocation for history. In-memory chart resets on reboot, independent of SD sessions and pause/stop.
- Existing H/J/K/D history/navigation and P/S/R recording controls remain enabled. Toggling A out of history closes any detail-file handle and returns to the dashboard or activity screen; live scanning and SD recording continue.

## Hardware checklist

1. Compile `pio run -e cardputer-adv` and upload; open with A, close with A.
2. Watch graph counts grow with Wi-Fi/BLE scans and LoRa receptions. Confirm bars age out after two minutes and the visual remains legible at 240×135.
3. During RF view press P/S/R, inspect SD status and JSONL integrity, and verify Wi-Fi/BLE alternation, LoRa RX, GNSS freshness and keyboard responsiveness.
4. Press H and navigate history/details; switch back using A and confirm no file handles remain open.
5. Leave running for 30 minutes to inspect heap, `PIPE dropped`, BLE prequeue drops and SD errors.

**Not compiled or tested on physical hardware by the assistant.**
