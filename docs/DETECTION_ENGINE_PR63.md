# PR63 — Detection engine foundation

This change provides a **header-only, allocation-free C++ classifier** for the Cardputer. It deliberately does **not** include any live Flock/ALPR/Axon vendor signature or automatic alerts yet. Those require verified identifiers, provenance, input enrichment and false-positive tests in subsequent PRs.

## Types
- `DetectionType`: Unknown, Flock, Alpr, Axon, Camera, Tracker, Meta, Skimmer.
- `EvidenceType`: Wi-Fi SSID/BSSID/management or BLE name/manufacturer/service.
- `MatchStrength`: weak, moderate, strong, chosen by future **vetted** rules.
- `Detection`: classification, confidence, evidence count and independent evidence modalities.

Evidence must include stable rule ID **and** source/provenance. Same-type observations cannot raise confidence through repetition. Single evidence can be low or medium only; high requires strong evidence in one modality plus another modality. That is a *policy score*, **not a statistical probability**, and does not prove a device's identity. Beware that multiple advertising fields can be controlled by the same transmitter, so independence is only partial.

## Host tests
```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/detection_engine_test.cpp -o /tmp/callingcard-detection-test
/tmp/callingcard-detection-test
```
No embedded dependencies are needed. The normal `cardputer-blade-details` firmware and the Android HUD remain untouched, so this PR cannot introduce new BLE connection or scan behavior. The engine is not yet connected to the event pipeline.

## Planned follow-up
PR64: collect Wi-Fi BSSID and create carefully sourced, tested rules; PR65 BLE advertisement fields; PR66 transmit classified alerts on the current unencrypted link. Until then **no Flock, ALPR, or surveillance camera detection is operational**.

Do not transmit exact location, stable device identifiers, or secrets on the unencrypted HUD link.
