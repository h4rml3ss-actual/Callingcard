# PR 11 — Passive Meshtastic US LongFast radio test

This is a **PHY-only listening profile**, not a Meshtastic client. It does not join a mesh, transmit, decrypt, parse packet content, or retain LoRa payloads. Incoming packets generate only the existing `CAP LORA RX count=... rssi=... snr=...` metadata.

## Profile selection

Edit `platformio.ini`:

- `-DCALLINGCARD_LORA_PROFILE=1` (default): **MESHTASTIC_US_LONGFAST**, 906.875 MHz, BW 250 kHz, SF11, coding rate 4/5, sync `0x2B`, preamble 16 symbols.
- `-DCALLINGCARD_LORA_PROFILE=0`: **LEGACY_915_TEST**, 915 MHz, BW 125 kHz, SF9, coding rate 4/5, sync `0x12`, preamble 8 symbols.

Changing the flag requires rebuilding and reflashing. A startup line identifies the exact active profile.

**Important:** 906.875 MHz comes from the supplied *specific device's* Meshtastic US LongFast serial log, not a universal frequency for every Meshtastic deployment. Region, preset, frequency slot, and override settings must match to receive packets. These settings do not provide Meshtastic packet interpretation.

## ADV hardware validation

1. Power down before mounting the Cap and **install the correct antenna before powering it**.
2. Build: `pio run -e cardputer-adv`; upload using PlatformIO. Open serial monitor at 115200.
3. Reboot with monitor open if needed to catch `CAP LORA PROFILE`. Verify `name=MESHTASTIC_US_LONGFAST freq_mhz=906.875 bw_khz=250.0 sf=11 cr=4/5 sync=0x2B preamble=16 rx_only=1`.
4. Verify recurring `CAP DIAG stage=READY sx1262_init=0 rx_start=0 radio_state=0` (other fields may appear between).
5. Listen where you previously received legitimate Meshtastic LongFast transmissions on **that frequency slot**. Look for `CAP LORA RX count=...`. No packets does not prove a hardware failure: there may be no compatible transmissions, frequency offsets, antenna-switch constraints, or airtime contention.
6. Verify Wi-Fi, BLE, GNSS stats, dropped queue counts, and heap remain stable. Collect ~30 minutes of logs.
7. For a regression comparison, set `CALLINGCARD_LORA_PROFILE=0`, rebuild, reflash, and verify the legacy profile line. This profile intentionally does not match Meshtastic LongFast.

## Outstanding hardware question

The Cap documentation specifies PI4IOE P0 controls an RF switch, while the observed working Meshtastic firmware configures SX1262 DIO2. Callingcard still sees no I²C ACK at `0x43`, so its external antenna switch is **unverified**, even when RadioLib reports successful initialization. PR 11 does not alter antenna power wiring or enable TX.

## Development status

Changes were authored remotely. **Not compiled or bench-tested by the assistant.** RadioLib is GitHub-sourced and presently unpinned. Successful driver initialization plus actual packet-reception logs are the acceptance criteria.
