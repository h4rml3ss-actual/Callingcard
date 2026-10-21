#pragma once
#include <Arduino.h>
#include "Observation.h"
#include "SessionStats.h"

namespace callingcard {
// One producer context at a time in current firmware; callbacks during BLE scan
// and main loop do not concurrently mutate queue. Revisit if adding RTOS tasks.
class EventPipeline {
 public:
  static constexpr uint8_t kCapacity = 32;
  bool enqueue(const Observation& observation);
  void drainTo(Print& output, uint8_t maximum = 8, Print* mirror = nullptr, SessionStats* stats = nullptr);
  uint32_t mirrorErrors() const { return mirrorErrors_; }
  uint32_t dropped() const { return dropped_; }
  uint32_t emitted() const { return emitted_; }
  uint8_t queued() const { return count_; }

 private:
  Observation queue_[kCapacity]{};
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  uint32_t dropped_ = 0;
  uint32_t emitted_ = 0;
  uint32_t sequence_ = 0;
  uint32_t mirrorErrors_ = 0;
  static void printEscaped(Print& out, const char* data);
  void printEvent(Print& out, const Observation& observation);
};
} // namespace callingcard
