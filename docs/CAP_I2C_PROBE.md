# PR 8 — LoRa-1262 I²C expander address probe

## Reported hardware observation
Cardputer ADV with Cap LoRa-1262 and antenna installed shows `LORA:ERR`. Recurring diagnostics report `stage=EXPANDER i2c=1 expander=0 sx1262_init=-999`. Meanwhile Wi-Fi/BLE operate and GNSS has thousands of checksum-valid NMEA sentences.

## Changes
Before initializing the PI4IOE5V6408 driver, scan the **existing M5Unified internal I²C controller** once at boot. Enumerate responding 7-bit addresses and specifically probe the documented Cap expander address **0x43**. Do not create a second Arduino Wire instance that could reconfigure I²C. Retain results for the recurring diagnostics:

```text
CAP I2C found=0x43
CAP I2C probe target=0x43 ack=1 devices=...
CAP I2C status initialized=1 target_0x43_ack=1 devices=... expander_driver=...
CAP DIAG stage=...
```

These lines are illustrative. If `ack=0`, the code **does not** attempt the expander driver or SX1262. If `ack=1` but `expander_driver=0`, I²C connectivity exists but driver initialization still fails. If both pass, SX1262 initialization is attempted.

## Hardware procedure
1. With the Cap securely seated and antenna attached, pull the change, build and flash in PlatformIO VS Code.
2. Keep PlatformIO Monitor at 115200. Initial USB messages may be missed, but `CAP I2C status` repeats every 10 seconds.
3. Capture one complete `CAP I2C status` and `CAP DIAG` line. Note any one-time `CAP I2C found` addresses if visible.
4. If `0x43_ack=0`, check seating, board revision, I²C wiring/voltage and manufacturer pinout. Confirm the exact Cap model. Do **not** bypass the expander requirement.
5. If `0x43_ack=1` and `expander_driver=0`, inspect PI4IOE library version, bus frequency and begin() behavior.
6. If the expander succeeds and SX1262 fails, use its numeric RadioLib error to continue SPI troubleshooting.
7. Verify Wi-Fi, BLE and GNSS logging still operate and no reboot loop occurs.

## Limitations
- **Not compiled or hardware tested in this PR.** Compatibility of M5Unified `scanID(bool[120])` must be verified against the locally resolved dependency revision.
- The bus scan runs **once during startup**, not in the main loop; avoiding repeated unsolicited I²C traffic is intentional.
- Successful I²C `begin` does not establish an expander is present. A device ACK at 0x43 also does **not** prove that the PI4IOE driver can operate correctly.
- No LoRa transmit behavior, frequency changes, SPI wiring changes, or GNSS fixes are introduced.
- If the SPI/SD coexistence later requires bus management, address it in a distinct PR.

Manufacturer reference: https://docs.m5stack.com/en/arduino/projects/cap/cap_lora868
