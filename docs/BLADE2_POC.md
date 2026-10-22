# PR 30 — Blade 2 direct BLE telemetry proof of concept

**Goal:** Test whether Vuzix Blade 2 can run a native Android BLE central application that directly reads aggregate status from the Cardputer ADV BLE GATT peripheral. This does **not** use Vuzix's phone companion SDK.

Vuzix Blade 2 is Android 11, compatible with Android BLE APIs. Firmware uses the existing ESP32 Arduino BLE stack. This remains a **hardware hypothesis** until verified: simultaneously advertising GATT telemetry and scanning other BLE devices may trigger coexistence limitations.

## Firmware

Default firmware is **unchanged** and does not advertise telemetry. Use the opt-in PlatformIO profile:

    pio run -e cardputer-blade-poc
    pio run -e cardputer-blade-poc -t upload

To return to stable configuration:

    pio run -e cardputer-adv -t upload

The opt-in firmware advertises BLE GATT service `cabb1e00-ccad-4abc-a112-24fc00000001`, characteristic `cabb1e00-ccad-4abc-a112-24fc00000002` (READ + NOTIFY, CCCD). Each ~1s status payload is ASCII `1,wifi,ble,lora,gps_fix,recording`; example: `1,8,23,2,0,1`. Fields are latest Wi-Fi scan count, latest BLE unique scan count, cumulative LoRa receptions (modulo 1000), boolean GNSS fix, boolean SD recording. Version 1 counts are **0–999** for limited ATT MTU. No GPS coordinates, SSIDs, advertised names, packet payloads or stored logs are sent.

## Blade 2 application

Open the `blade2/` directory in Android Studio as a Gradle project; install Android SDK 34. It contains a Java Android app (minSdk 30) and requests BLE scan/connect runtime permissions as appropriate. It scans by service UUID, connects, discovers services, enables CCCD notifications and displays incoming values on a simple full-screen HUD. No Vuzix-specific proprietary API or smartphone bridge needed for this trial.

Build in Android Studio, install the APK using the Blade 2 developer/debugging workflow over USB/ADB, enable Bluetooth on glasses and launch **Callingcard HUD**. On the Cardputer, leave the POC environment running and monitor USB serial.

## Acceptance gate

1. Verify both `pio run -e cardputer-adv` and `pio run -e cardputer-blade-poc` compile.
2. Verify Blade APK builds and installs, app displays a permission prompt, and scans for the service without a phone.
3. Confirm app shows counters and they change as Cardputer observes new Wi-Fi/BLE/LoRa activity.
4. Disconnect/reconnect Blade, restart either device and confirm reconnection.
5. Compare BLE discovery counts, queue drops, prequeue drops, GNSS freshness, LoRa RX and SD recording before and after enabling telemetry. **If BLE scanning stops or loses significant data, turn off the POC by flashing normal cardputer-adv firmware.**
6. Check no coordinates or device identifiers are emitted over the BLE telemetry characteristic.

## Security / limitations

**Prototype only.** BLE GATT service is unauthenticated, unencrypted, and readable by other nearby BLE clients. Therefore it sends low-sensitivity aggregate counts only. Do **not** transmit sensitive location, device names, security alerts, or remote control commands until we add peer authentication/encryption and authorization. The Blade app does not pin an authenticated device identity; it connects to the first matching advertisement. Future work: verified pairing/bonding, secure GATT, lifecycle handling, status freshness timeout, and interoperability/coexistence optimization.

Android Gradle Plugin may require Android Studio to install a compatible Gradle distribution. No committed wrapper JAR is included. Source and tests were written through GitHub and **not compiled or physically validated by the assistant**.

References:
- https://support.vuzix.com/docs/technical-details
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ble.html
