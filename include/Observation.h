#pragma once
#include <stdint.h>

namespace callingcard {
// Lightweight, fixed-size observation snapshot. Not persistent device identity.
// In PR3 observations are emitted on serial and shown on screen only.
enum class Kind : uint8_t { WifiNetwork, BleAdvertisement };
struct Observation {
  Kind kind = Kind::WifiNetwork;
  uint32_t uptimeMs = 0;
  int16_t rssiDbm = 0;
  int16_t channel = -1;
  char label[24] = {};
};
inline const char* kindName(Kind kind) {
  return kind == Kind::WifiNetwork ? "wifi" : "ble";
}
} // namespace callingcard
