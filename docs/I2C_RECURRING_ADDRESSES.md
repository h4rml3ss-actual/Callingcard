# Recurring I²C Address Diagnostics (PR #9)

The original I²C scan printed device addresses once during boot, but PlatformIO Monitor on macOS can miss those messages when USB CDC reconnects. The firmware now retains the full scanned address set in a fixed-size 128-element boolean array and repeats it alongside existing CAP diagnostic output (about every 10 seconds).

## PlatformIO test

1. Pull the branch after merging the PR; select `cardputer-adv` in PlatformIO.
2. Build, upload, and start Monitor at **115200 baud**.
3. Find `CAP I2C addresses:` in the serial output. For example:
   ```text
   CAP I2C status initialized=1 target_0x43_ack=0 devices=3 expander_driver=0
   CAP I2C addresses: 0x34 0x51 0x68
   ```
   These are *illustrative* addresses, not measured results.
4. Share the actual `CAP I2C addresses:` line and corresponding `CAP DIAG` line for review.

The address list is a **snapshot of the one-time boot scan**, not a continuously rescanned bus. No new I²C transactions are performed during recurring logging. `0x43` is the anticipated LoRa-1262 Cap expander address. If absent, SX1262 initialization is deliberately skipped; this change does not modify I²C pins, frequency, or radio settings.

**Validation status:** Changes committed to the repository; compilation, upload, and hardware operation have not been verified by the assistant.
