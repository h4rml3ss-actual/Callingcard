# PR 4: LoRa-1262 Cap hardware bring-up

This PR adds a **receive-only** SX1262 and GNSS hardware spike to the existing alternating Wi-Fi/BLE scanner. No LoRa transmission, Meshtastic message decoding, SD logging, Vuzix connection, or fused geotagging is included.

## Verified hardware mappings (manufacturer)
- SX1262 SPI SCK 40, MISO 39, MOSI 14, CS 5; IRQ 4, RESET 3, BUSY 6.
- GNSS module TX is connected to host RX **GPIO15**, module RX to host TX **GPIO13**, default **115200 baud**.
- The **Cap LoRa-1262**, unlike the legacy LoRa868 Cap, requires its PI4IOE5V6408 IO expander (I²C GPIO8/9, address 0x43) P0 driven high for the RF antenna switch.
- **SD card and SX1262 share SPI pins**. SD logging is intentionally deferred until bus sharing / CS handling are validated.

Manufacturer references:
- https://docs.m5stack.com/en/cap/Cap_LoRa-1262
- https://docs.m5stack.com/en/arduino/projects/cap/cap_lora868
- https://docs.m5stack.com/en/core/Cardputer_Mesh_Kit

## Bring-up
1. Physically secure Cap and its correct antenna **before powering**.
2. Attach USB data cable, ensure a recoverable firmware image and correct board selected.
3. Build: `pio run -e cardputer-adv`
4. Flash: `pio run -e cardputer-adv -t upload`
5. Monitor: `pio device monitor -b 115200`
6. Confirm `CAP SX1262 init status=0`, `CAP GPS bytes` increasing and Wi-Fi/BLE scans still alternating.
7. Outdoors, ensure `location=FIX` eventually becomes available. Indoor GNSS no-fix is not necessarily a fault.
8. To test RX, use a **known, authorized transmitter** matching 915 MHz, BW 125 kHz, SF9, CR 4/5, sync 0x12 and the other configured radio settings. This code never transmits. Missing RX packets alone does not indicate a malfunction.

## Known limitations and risks
- **Not yet compiled or flashed by the assistant**; manufacturer-linked libraries and exact RadioLib APIs must be verified on local PlatformIO.
- Existing Wi-Fi/BLE scans are synchronous, so GNSS UART processing is delayed during them. A 2048-byte RX buffer only mitigates drops; don't interpret timestamps or fix ages as continuous sample capture.
- Raw LoRa payload contents are not persisted or printed. The LoRa receive handler currently uses a short main-loop poll and prints RSSI/SNR/packet count.
- The RF expander and radio may require coordinated lifecycle handling after sleep/reboot.
- On-screen GNSS indicator uses fresh valid location only; no stale coordinates are presented as current.
- 915 MHz is a **receive-only US test default**, not a universal compatible setting and not an assertion about regulatory requirements.
- Memory/runtime and GPIO conflicts must be measured. The display layout is narrow; consider dedicated status pages in a later PR.
- Both Wi-Fi scan and BLE scan still contain blind periods. A failed SX1262 init must not stop standalone Wi-Fi/BLE collection.

## Acceptance
- [ ] PlatformIO clean build on supported environment
- [ ] ADV screen and serial boot
- [ ] Wi-Fi/BLE surveys continue with Cap connected
- [ ] I2C expander and SX1262 return successful statuses
- [ ] GNSS serial bytes increment; outdoors current location fix recorded
- [ ] Compatible, known RX packet increments receive count
- [ ] No crashes/heap degradation over a 30-minute test
- [ ] Confirm antenna and SD SPI ownership before next PR

## Licensing
Callingcard still does not embed SquachWatch. M5Stack examples were consulted for pin configuration and API selection, but this code is independently organized.
