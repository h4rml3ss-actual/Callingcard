#pragma once
#include <Arduino.h>
#include <SD.h>

namespace callingcard {
// Bounded history index. Scans at most 6 possible names per tick and keeps
// the latest 16 existing files, without allocating memory or reading JSONL.
class SessionHistory {
 public:
  static constexpr uint8_t kCapacity = 16;
  struct Entry { uint16_t number = 0; uint32_t bytes = 0; };
  void begin() {
    count_ = 0; selected_ = 0; nextId_ = 0; scanning_ = true;
  }
  void tick(const char* activePath) {
    if (!scanning_) return;
    for (uint8_t n = 0; n < 6 && nextId_ < 1000; ++n, ++nextId_) {
      char path[32];
      snprintf(path, sizeof(path), "/CALLINGCARD/S%03u.JSONL", nextId_);
      if (!SD.exists(path)) continue;
      // Never open the active file for competing reads; size is unknown while recording.
      if (activePath && strcmp(activePath, path) == 0) continue;
      File f = SD.open(path, FILE_READ);
      if (!f) { ++errors_; continue; }
      const uint32_t bytes = f.size();
      f.close();
      if (count_ < kCapacity) {
        entries_[count_].number = nextId_;
        entries_[count_].bytes = bytes;
        ++count_;
      }
      else {
        for (uint8_t i = 1; i < kCapacity; ++i) entries_[i-1] = entries_[i];
        entries_[kCapacity-1].number = nextId_;
        entries_[kCapacity-1].bytes = bytes;
      }
    }
    if (nextId_ == 1000) {
      scanning_ = false;
      if (count_) selected_ = count_-1; // most recently numbered (not newest mtime)
    }
  }
  void move(int delta) {
    if (!count_) return;
    int newIndex = static_cast<int>(selected_) + delta;
    if (newIndex < 0) newIndex = 0;
    if (newIndex >= count_) newIndex = count_-1;
    selected_ = static_cast<uint8_t>(newIndex);
  }
  const Entry* selected() const { return count_ ? &entries_[selected_] : nullptr; }
  uint8_t count() const { return count_; }
  uint8_t selection() const { return selected_; }
  bool scanning() const { return scanning_; }
  uint16_t progress() const { return nextId_; }
  uint32_t errors() const { return errors_; }
 private:
  Entry entries_[kCapacity]{};
  uint8_t count_ = 0, selected_ = 0;
  uint16_t nextId_ = 0;
  bool scanning_ = false;
  uint32_t errors_ = 0;
};
} // namespace callingcard
