# PR 13 — microSD JSONL sessions (experimental)

The Cardputer ADV microSD pins are CS=GPIO12, CLK=40, MISO=39, MOSI=14; the Cap SX1262 shares these signals with **separate CS=GPIO5**. Sources: [M5Stack Cardputer ADV](https://docs.m5stack.com/en/core/Cardputer-Adv) and [Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262).

## Behavior
- On boot, after radio initialization, mount a FAT-formatted SD at a conservative 4 MHz SPI clock. Leave the radio chip deselected during mounting.
- Create a unique `/CALLINGCARD/S000.JSONL` through `S999.JSONL` name (first unused filename). Sessions are numbered, **not wall-clock named**.
- The existing single observation queue serializes each event only once, then writes identical bytes to Serial and, when available, SD.
- Process at most eight observations per loop, flush approximately every 5 seconds, stop file recording at 4 MiB per boot session. Logging writes are synchronous; this **does not guarantee interruption-free radio reception** during slow SD operations.
- Without a card, after mount error, or on detected short write: **continue live Wi-Fi/BLE/LoRa/GNSS and Serial JSONL**.
- Serial `SD status=...` lines report recording state, filename, bytes, and errors. Payload privacy remains as in PR #12 (LoRa metadata only).
- Power loss between flushes may lose buffered records; physical card corruption and latency must be validated before field reliance. Do not hot-remove SD or Cap while powered.

## ADV hardware checklist
1. Attach the LoRa antenna and Cap with power disconnected. Insert a FAT32 microSD card; back up its existing contents.
2. `pio run -e cardputer-adv` and flash; monitor serial at 115200. Look for `SD status=RECORDING path=/CALLINGCARD/S000.JSONL` (or a later number).
3. Run several minutes, verifying `CAP DIAG stage=READY`, `CAP LORA RX` events with a compatible signal, GNSS checksums increasing, and alternating Wi-Fi/BLE scanning.
4. Power off normally after allowing a 5-second flush interval; read SD on a computer. Validate each line is JSON, `seq` increments, and file lines match the serial JSON events for the same runtime (capture serial to a file for exact comparison).
5. Test boot **without** SD: look for `SD status=NO_CARD_OR_MOUNT_FAILED`, and ensure all radio features and JSONL serial remain operational.
6. Check file naming on a second boot and avoid overwriting an old session; test card full/write failure and 4 MiB session limit.
7. Perform a 30-minute soak with SD installed: monitor `PIPE dropped`, `CAP LORA RX`, heap, and session size. Evaluate missed packets during SD access compared with PR #12.

## Constraints
Firmware is authored but **not built or tested by the assistant**. A single main-loop producer/consumer and separate SPI chip-selects provide baseline ordering, but RadioLib may service receive SPI immediately following its IRQ flag; filesystem operations can still introduce latency. No RTOS-level mutex or robust recovery of a failing card is implemented. This implementation favors maintaining serial output over reliable archival: a failed mirror write increments an error counter, disables later SD mirroring, and may leave a partial final record. A future PR should add storage buffering / health checks, session controls, and safe shutdown.
