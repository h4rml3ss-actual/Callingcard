# PR 10 — SX1262 bring-up without expander gate

## Evidence motivating the change

On the user's Cardputer ADV + Cap LoRa-1262, Callingcard detects I²C addresses `0x18 0x34 0x69` but no `0x43`; its previous code returned early without calling `SX1262::begin()`. The **same physical hardware** running Meshtastic logged `SX126x init result 0`, `SX1262 init success`, and actual LoRa TX/RX. Meshtastic's startup also logged only I²C addresses `0x34` and `0x69`. This is strong evidence the SPI radio can initialize without a successful `0x43` scan.

## Behavior

- Continue to initialize GNSS UART independently.
- Initialize I²C and scan addresses once at startup, retain and print those results.
- When `0x43` ACKs and the expander driver initializes, set P0 high as before.
- **Otherwise log `mode=UNVERIFIED` and continue with a receive-only SX1262 SPI initialization attempt.** No bypass write is made to an unknown I²C device.
- Once `radio.begin` succeeds, request DIO2 RF switch mode (also configured within RadioLib's SX126x initialization) and log its numeric result before attempting `startReceive`.
- Keep the existing Wi-Fi, BLE, GNSS, JSONL, LoRa diagnostic and packet metadata behavior.

## Important limitations

A successful SX1262 SPI initialization is **not proof the antenna path is enabled**. The hardware documentation states that the PI4IOE P0 controls an external RF switch. The Meshtastic reference also configures DIO2 as an RF switch. Whether both switch-control mechanisms must be set on this particular Cap revision still requires verification against the board schematic and hardware reception testing. This PR **does not transmit** or change the existing test receive frequency, modem settings, or packet decoding. Keep the correct antenna attached.

## VS Code / PlatformIO verification

1. With the Cap seated and antenna attached, save the current working Meshtastic firmware version and settings if you wish to restore it later.
2. Pull/checkout the change in VS Code, then PlatformIO → cardputer-adv → **Build**.
3. If build succeeds, **Upload**, then **Monitor** at 115200.
4. Look for recurring `CAP DIAG` lines and a one-time `CAP EXPANDER` and `CAP SX1262 init status`; these can be missed on USB reconnect, so `CAP DIAG sx1262_init` is authoritative.
5. A result `stage=READY sx1262_init=0 rx_start=0` establishes driver initialization and receive mode. It does **not** establish that the RF path receives signals.
6. Test reception with an authorized LoRa transmitter with **exactly matching** 915 MHz, 125 kHz BW, SF9, coding rate 4/5, sync word 0x12, preamble and CRC assumptions. Meshtastic's logged US LongFast profile uses **906.875 MHz and 250 kHz BW**, so those packets are **not** expected to be received by Callingcard's current profile.
7. Run a 30-minute soak test for Wi-Fi/BLE stability, GNSS parsing, free heap, and radio receive-state errors.

## Expected limitations and safety

- **Not built or tested by the assistant.** The installed RadioLib version is unpinned; verify `setDio2AsRfSwitch` compiles and works with the resolved package.
- Only run this firmware with the correct Cap and antenna, and do not infer the expander is electrically absent solely from `0x43` not acknowledging.
- Treat `UNVERIFIED` as a hardware-path warning, not success. This code does not attempt LoRa transmit.
- Later work should compare the actual Meshtastic board variant source and Cap schematic, including how RF switch power and receive path are enabled.
