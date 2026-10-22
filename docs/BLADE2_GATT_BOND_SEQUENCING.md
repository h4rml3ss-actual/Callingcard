# Blade 2 pairing state: connect first, bond once (PR #44)

The previous Android app called `createBond()` as soon as the peripheral advertisement was received, before opening the BLE GATT link. Some BLE stacks need an established link for bond negotiation. PR #44 instead creates a single GATT connection first, starts pairing once after STATE_CONNECTED, and waits for BOND_BONDED before negotiating MTU and subscribing to protected characteristics. If already bonded, it skips first-time pairing and goes directly to discovery.

The ESP32 secure profile, MITM/encrypted GATT permissions and existing scanner/coexistence functionality are unchanged. No unrestricted telemetry fallback is added.

## Validation

1. Merge PR #44, pull and rebuild/reinstall only the `blade2/` Android app; keep the currently flashed `cardputer-blade-secure` firmware.
2. Restart the Android app. Observe a single PIN request, then a CONNECTED/BONDED transition, or note exactly which step fails.
3. If a stale bond is known to be invalid, use the Blade's Bluetooth settings to Forget Callingcard once before retrying. Do not erase Cardputer flash.
4. Capture Android `adb logcat -s CallingcardBlade BluetoothGatt AndroidRuntime` and Cardputer serial lines `BLADE secure_auth=...` and `BLADE connection_changed=...`, **redacting PINs, addresses, SSIDs and bond data**.
5. Verify telemetry on the glasses only after authentication; then restart both devices and confirm automatic reconnection without a new PIN.
6. If BLE GATT status reports failure or the ESP32 reports `secure_auth=0`, preserve the logs and stop retrying enrollment rather than repeatedly rotating bonds. A known-good legacy profile is available, but it does not have the new security controls.

**Unverified**: This change targets a plausible Android sequencing issue, not a proven unique root cause. The BLE stack might instead be failing MITM negotiation, CCCD authorization or persistent bond storage. Do not treat it as secured until negative tests demonstrate unauthenticated reads fail.

No Android build/hardware test was run by assistant.
