#pragma once
#include <Arduino.h>
#include "Observation.h"

namespace callingcard {
// A rolling history of enqueued observations (not unique devices or RF energy).
// 24 five-second bins, three radios; 288 bytes of counters.
class RfActivity {
 public:
  static constexpr uint8_t kBins = 24;
  static constexpr uint32_t kBinMs = 5000;
  void add(const Observation& o) {
    const uint32_t tick = o.uptimeMs / kBinMs;
    rotate(tick);
    const uint8_t source = static_cast<uint8_t>(o.kind);
    if (source >= 3) return;
    const uint8_t bin = tick % kBins;
    if (samples_[source][bin] != UINT32_MAX) ++samples_[source][bin];
  }
  void advance(uint32_t now) { rotate(now / kBinMs); }
  uint32_t sample(uint8_t source, uint8_t oldestToNewest) const {
    if (source >= 3 || oldestToNewest >= kBins || !initialized_) return 0;
    const uint32_t tick = currentTick_ - (kBins - 1 - oldestToNewest);
    // Slots older than boot / initialized frame are zeroed by rotate.
    return samples_[source][tick % kBins];
  }
  uint32_t sum(uint8_t source) const {
    uint32_t total = 0;
    for (uint8_t i=0; i<kBins; ++i) total += sample(source,i);
    return total;
  }
  uint32_t peak(uint8_t source) const {
    uint32_t peakValue = 0;
    for (uint8_t i=0; i<kBins; ++i)
      if (sample(source,i)>peakValue) peakValue = sample(source,i);
    return peakValue;
  }
 private:
  void rotate(uint32_t tick) {
    if (!initialized_) {
      currentTick_ = tick; initialized_=true;
      return;
    }
    if (tick <= currentTick_) return; // Late observations count in their existing bin.
    const uint32_t delta = tick - currentTick_;
    for (uint32_t offset=1; offset<=delta && offset<=kBins; ++offset) {
      const uint8_t bin = (currentTick_ + offset) % kBins;
      for (uint8_t radio=0; radio<3; ++radio) samples_[radio][bin]=0;
    }
    if (delta >= kBins) {
      for (uint8_t radio=0;radio<3;++radio)
        for(uint8_t bin=0;bin<kBins;++bin) samples_[radio][bin]=0;
    }
    currentTick_ = tick;
  }
  uint32_t samples_[3][kBins]{};
  uint32_t currentTick_ = 0;
  bool initialized_ = false;
};
} // namespace callingcard
