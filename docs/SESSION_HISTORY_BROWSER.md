# PR 23 — On-device session history

Press **H** to enter or leave the microSD session history screen. **J** selects the preceding indexed file, and **K** selects the next. Existing **P/S/R** recording controls continue to work even while the history page is open.

## Implementation and limits

- Iterates over the existing fixed filename convention `/CALLINGCARD/S000.JSONL` through `S999.JSONL` in **six candidate checks per main-loop iteration**, rather than blocking while scanning the whole directory.
- Keeps only the **16 highest-numbered prior recordings** in a fixed-size RAM index. No heap allocated for catalog entries. Displays session filename and file size, plus scan progress/read errors.
- The active recording file is excluded from history indexing: no concurrent read of a file being appended to. Once a session is closed, re-enter history with **H** to rescan it.
- Entries are ordered by **numeric session ID**, not modification time; previous firmware writes no persistent summary per session. This PR **does not** reconstruct per-session Wi-Fi/BLE/LoRa counts from historical JSONL files. Detailed historical summaries will require a bounded background parser or compact summary sidecar in a later PR.
- The screen is read-only: no deletion, modification, exporting, or replay; log files remain unchanged.
- The scanner, GNSS, LoRa receive and microSD recording still run while browsing. SD directory operations occur only from the main loop, serialized with logging; slow media may nevertheless increase loop latency.
- If no card is mounted, H leaves the normal dashboard untouched.

## Acceptance checks

1. Build `pio run -e cardputer-adv` and upload. Open history with **H**, observe incremental indexing, and navigate using **J/K**.
2. With several closed `Sxxx.JSONL` files on the card, compare filenames/sizes with files observed on a computer. Confirm at most 16 are indexed.
3. While recording, open the history page, continue monitoring Wi-Fi/BLE/LoRa and SD JSONL on serial, and test **P/S/R**.
4. Stop recording, exit and reopen history. Confirm the newly closed file is indexed and current open session remains excluded.
5. Test empty card or no-card boot; history should show a useful empty state or remain disabled.
6. Check SD errors, heap, GNSS freshness, BLE callback-buffer loss and queue drops during a 30-minute soak.

**Status:** Code has been committed but not compiled or hardware-tested by the assistant.
