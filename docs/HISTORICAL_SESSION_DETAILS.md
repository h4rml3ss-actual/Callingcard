# PR 25 — Historical session details

Open the existing history browser with **H**, choose a closed recording with **J/K**, then press **D** to inspect it. **H** from the detail page returns to the history list; **H** again exits. Existing P/S/R session controls are unchanged.

## Reader behavior
- Opens the selected `/CALLINGCARD/Sxxx.JSONL` file read-only; no active recording can be selected.
- Parses at most **512 bytes per loop iteration**, with a **512-byte line buffer** and no unbounded file reads. This allows the radio, keyboard, GNSS, display and logging main loop to keep running. SD reads may still incur small latency spikes.
- Displays processed Wi-Fi/BLE/LoRa observation counts, count of lines containing UTC, count containing GPS position, file progress, invalid/truncated line count and a Wi-Fi or BLE RSSI range.
- This is a **format-specific lightweight field reader** for `callingcard.event.v0`, not a general-purpose JSON validator. It matches stable field keys from the current serializer, so malformed JSON with familiar key substrings may be counted. Oversized JSONL lines and final unterminated lines are reported as invalid. Statistics count saved JSONL observation rows, not discovered physical devices.
- Only one file handle is held; it is closed when complete or when leaving the details screen. Session history already ignores the currently open recording.
- Older recordings written by earlier firmware should work if they use `callingcard.event.v0`.

## Validation
1. Build with `pio run -e cardputer-adv` and upload.
2. Open history with H; wait for indexing, select a closed file with J/K, press D.
3. Verify progress increases without freezing live BLE/Wi-Fi/LoRa/GNSS; H returns to history without modifying the file.
4. Confirm parsed counts against a sample file on a computer (e.g. count lines with source wifi/ble/lora and non-null utc/position).
5. Compare bad/truncated count when supplied an empty file and a deliberately truncated last line.
6. Verify SD logging continues while a closed file is inspected, and no SD error or heap collapse during a 30-minute run.

**Validation status:** Changes have been committed via GitHub but are not compiled or tested on physical Cardputer hardware by the assistant.
