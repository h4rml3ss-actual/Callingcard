#pragma once
#include <stdint.h>

namespace callingcard {
// Lightweight, fixed-size observation snapshot. Not persistent device identity.
// In PR3 observations are emitted on serial and shown on screen only.
enum class Kind : uint8_t { WifiNetwork, BleAdvertisement, LoRaPacket };
struct Observation {
  Kind kind = Kind::WifiNetwork;
  uint32_t uptimeMs = 0;
  int16_t rssiDbm = 0;
  int16_t channel = -1;
  float snrDb = 0.0f; // Only meaningful for LoRaPacket.
  char label[24] = {};
  char wifiBssid[18] = {}; // Wi-Fi network identifier captured at observation time.
  char bleAddress[18] = {}; // BLE advertiser address may be randomized/rotate.
  bool bleHasCompanyId = false;
  uint16_t bleCompanyId = 0; // Advertising company identifier (not device identity).
  char bleServiceUuid[37] = {}; // First advertised service UUID, if any.
  // Snapshot at observation creation; never retroactively enrich queued events.
  // Classified candidate captured at observation time, not inferred later.
  bool hasDetection = false;
  char detectionType[12] = {};
  char detectionConfidence[8] = {};
  char detectionRule[48] = {};
  bool hasUtc = false;
  char utc[25] = {}; // UTC ISO8601 second precision, Z suffix.
  bool hasPosition = false;
  double latitude = 0.0;
  double longitude = 0.0;
};
inline const char* kindName(Kind kind) {
  switch (kind) {
    case Kind::WifiNetwork: return "wifi";
    case Kind::BleAdvertisement: return "ble";
    case Kind::LoRaPacket: return "lora";
  }
  return "unknown";
}
} // namespace callingcard
