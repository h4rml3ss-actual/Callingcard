# PR57: NimBLE pairing PIN visibility

Use the **isolated** `cardputer-nimble-pairing-test` profile only. This does not modify main scanner firmware, Blade app, CSTP crypto, or existing BLE authentication policies.

## Verify

```sh
pio run -e cardputer-nimble-pairing-test
pio run -e cardputer-nimble-pairing-test -t upload
pio device monitor -e cardputer-nimble-pairing-test
```

The Cardputer ADV screen displays **PAIRING PIN** and the six-digit code. While not authenticated, the same configured code prints to USB every five seconds, including if the monitor attaches after boot:

```text
NIMBLE PAIRING PIN=****** connected=0 passkey_callback=0
```

`passkey_callback=1` means NimBLE invoked `onPassKeyDisplay()`. A PIN visible on screen does **not** mean the passkey callback or authenticated pairing completed. On Blade, open **Callingcard HUD**, enter the shown PIN when prompted and wait for:

```text
NIMBLE auth encrypted=1 mitm=1 bonded=1 accepted=1
```

Do not post unredacted PINs in PRs, screenshots or issue logs; the PIN is deliberately displayed/logged **only in the developer pairing test**. The PIN remains unchanged during a boot and changes on firmware restart; use the code currently visible on the device.

If the build fails, share the compiler errors; this PR hasn't been built or hardware-tested by the assistant. Preserve Secure Connections/MITM/bonding requirements and do not switch to Just Works. If pairing still fails, share the `passkey_callback` status, masked Android bond events, and NimBLE authentication result.
