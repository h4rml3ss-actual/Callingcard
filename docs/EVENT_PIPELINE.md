# PR 5: Bounded event queue and JSONL serial stream

## Scope
Introduce a fixed 32-record in-RAM FIFO for the PR3 Wi-Fi/BLE observations. Queue consumption serializes into newline-delimited JSON objects using schema `callingcard.event.v0`. The BLE advertisement callback **does not print**; it only enqueues. The serial drain runs between scan windows and is intentionally bounded at eight observations per loop.

Example (illustrative):
```json
{"schema":"callingcard.event.v0","seq":1,"kind":"observation","source":"wifi","uptime_ms":2145,"rssi_dbm":-64,"channel":6,"label":"test-network","utc":null,"position":null}
```

A limited number of Wi-Fi and BLE samples are captured per scan. These are **observations**, not stable device records, threat classifications, or a comprehensive packet log. Device names are not persisted to storage by this PR. Sequence numbers restart on reboot and are not globally unique.

## Why no microSD writes in this PR?
The SX1262 Cap and Cardputer SD share SPI wires. We must first verify exact chip-select ownership, bus settings and peripheral lifecycle on real hardware. Writing to SD on an unverified SPI configuration while a LoRa IRQ is active risks unreliable transfers. The UART stream is an integration/test sink and a necessary precondition for later durable logging.

## Build / smoke test
```sh
pio run -e cardputer-adv
pio run -e cardputer-adv -t upload
pio device monitor -b 115200
```
- Verify LCD still presents Wi-Fi/BLE and Cap status.
- Verify one JSON object per observation line and `schema=callingcard.event.v0`.
- Try a BLE name with quotes/backslashes; ensure JSON escaping.
- Verify `PIPE queue=N emitted=N dropped=N` periodic diagnostics.
- Run 30 minutes, record heap, overflow and watchdog resets.
- Verify Wi-Fi/BLE collection still alternates with LoRa and GNSS attached.
- With deliberate serial backpressure, confirm no memory growth; queue drops are allowed but must be counted.

## Known limitations
- **Code has not been compiled or run on the physical ADV**.
- One producer execution context is assumed; if callbacks move to separate FreeRTOS tasks, guard queue mutations using critical sections or a thread-safe FreeRTOS queue before enabling concurrent callers.
- Synchronous Wi-Fi/BLE scans pause GNSS processing; fix-to-observation timestamp correlation requires a later refactor.
- Serial output may block while draining; do not call from an ISR and measure scan timing.
- The FIFO holds at most 32 observations; screen still holds at most five per scan. Exceeding either limit results in deliberate sampling/truncation.
- The JSONL stream is not durable. A later PR will use explicit SD chip-select arbitration, session identifiers, retention settings, and write-failure handling.

## Exit criteria
A repeatable physical ADV build, valid JSONL output, bounded memory, visible drop accounting and 30-minute no-crash evidence. Do **not** represent a merged PR alone as hardware validation.
