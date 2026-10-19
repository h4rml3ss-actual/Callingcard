# LoRa-1262 startup diagnostics

The physical Cardputer ADV is scanning Wi-Fi/BLE and receiving GNSS UART bytes, but the LCD reports `LORA:ERR`. On macOS the PlatformIO serial port briefly disappears during reboot, so one-time startup messages may be lost.

This patch **does not alter radio hardware configuration**. It retains and repeats the initialization stage and numeric RadioLib error codes every 10 seconds, plus GPS NMEA checksum statistics. It also displays a compact stage and error number on the Cardputer LCD.

## Test
1. In VS Code, pull the merged PR, or check out the PR branch for testing.
2. PlatformIO → **cardputer-adv → Build**, then **Upload** and **Monitor** at 115200.
3. Look for a recurring line beginning `CAP DIAG` (the boot text can be missed).
4. Copy that full line. For example, `stage=EXPANDER i2c=1 expander=0` implicates the I²C expander, while `stage=SX1262_INIT i2c=1 expander=1 sx1262_init=-2` indicates failure inside RadioLib begin. The error codes here are **examples**, not measured values.
5. `gps_chars` is bytes seen by the parser; `gps_passed` / `gps_failed` track NMEA checksum results. Bytes increasing alone does not prove valid navigation sentences.
6. Confirm previous Wi-Fi/BLE scans and JSONL events continue.

## Fields
- `stage`: last initialization stage, `READY` after successful radio receive start, or `RX_RESTART` if subsequent receive restart fails.
- `i2c` / `expander`: successful completion of their begin calls.
- `sx1262_init`: result of `radio.begin` (sentinel `-999` when not attempted).
- `rx_start`: result of most recent receive-start attempt (sentinel `-999` if not attempted).
- `radio_state`: current effective readiness/error code.
- `gps_chars`, `gps_passed`, `gps_failed`, `gps_fix`: GNSS parser/position diagnostics.

The output is every ten seconds, not continuous. It remains available after reconnecting the USB serial monitor.

**Validation:** Code committed; PlatformIO compilation and physical hardware execution still require verification.
