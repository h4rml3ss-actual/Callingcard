# PR 14 — Session management and recording states

Callingcard now keeps surveying Wi-Fi/BLE/LoRa/GNSS independently from whether the microSD session is recording. An SD card inserted before boot auto-starts a new uniquely named session, preserving PR #13 behavior.

## Keyboard controls

The Cardputer keyboard is polled in the main loop using M5Cardputer's documented `isKeyPressed` API. Each key triggers on the transition from released to pressed, not continuously while held.

| Key | Action |
| --- | --- |
| **P** | Pause an active recording; flush pending data. Press P again to resume the same file. |
| **S** | Stop current recording, flush and close the file. |
| **R** | Resume a paused session or start a **new** numbered session after Stop or size limit. |

No current key is an emergency power-off control. The on-screen `SD:` row identifies the status and byte count; the serial `SD status=...` messages provide the full filename. All other scanning continues while paused/stopped.

## States

- **NO_CARD**: SD mount unsuccessful; serial-only. Reinsert the card with power off and reboot.
- **RECORDING**: same JSONL event bytes mirrored to SD as serial.
- **PAUSED**: no SD writes; file remains open and was flushed on pause.
- **STOPPED**: safely flushed/closed; R creates a new file.
- **LIMIT**: 4 MiB session cap reached; file flushed/closed; R creates a new file.
- **ERROR**: filesystem or write error. File closed, SD disabled for this session; inspect the card and reboot. Serial logging and surveys continue.

## Manual validation

1. With a FAT32 microSD, boot and confirm `SD status=RECORDING`. Verify Wi-Fi/BLE/LoRa activity and zero queue drops.
2. Press **P**; confirm `PAUSED` serial and display status, wait through additional Wi-Fi/BLE cycles. Files should not grow while paused.
3. Press **P** again to resume the same file; confirm bytes increase and the file retains valid JSONL records.
4. Press **S**; confirm `STOPPED`, disconnect power only after that line. Inspect SD contents and verify valid JSONL.
5. Boot again, press **S**, then **R**; ensure the new file uses the next unused `SNNN.JSONL` filename and never appends to the old one.
6. Reboot without the card, ensure `NO_CARD` and that the serial event stream plus scans remain operational.
7. Soak test while receiving LoRa traffic; monitor `CAP LORA RX`, `PIPE dropped`, heap, and SD errors.

## Limits and safety

- Keyboard checks run between blocking Wi-Fi/BLE survey calls; key presses shorter than those calls may be missed. This is not a guaranteed instant stop command.
- Pause flushes but **does not close** the file. Stop closes it. Do not pull the card while paused or recording.
- No guaranteed safe closure if the battery is removed or hard power is cut before stopping. SD calls are synchronous, sharing the same SPI bus as SX1262. Verify physical reception during active logging.
- Previous PR #13 SD output may still contain files with power-loss truncation; do not assume historical files were repaired.
- The code has not been compiled or tested on hardware by the assistant.
