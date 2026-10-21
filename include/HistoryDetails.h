#pragma once
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include <stdlib.h>

namespace callingcard {
// Read-only, incremental JSONL audit. No ArduinoJson or large allocations.
class HistoryDetails {
 public:
  void close() { if (file_) file_.close(); state_ = State::Idle; }
  bool open(uint16_t number) {
    close();
    reset();
    char path[32];
    snprintf(path, sizeof(path), "/CALLINGCARD/S%03u.JSONL", number);
    file_ = SD.open(path, FILE_READ);
    if (!file_) { state_ = State::Failed; return false; }
    number_ = number;
    bytes_ = file_.size();
    state_ = State::Reading;
    return true;
  }
  void tick() {
    if (state_ != State::Reading) return;
    // At most 512 bytes per iteration. A slow SD read can still add latency.
    uint16_t budget = 512;
    while (budget-- && file_.available()) {
      const int v = file_.read();
      if (v < 0) { state_ = State::Failed; break; }
      ++read_;
      if (v == '\n') {
        if (!oversize_ && used_) parseLine();
        else if (oversize_) ++invalid_;
        used_ = 0; oversize_ = false;
      } else if (!oversize_) {
        if (used_ < sizeof(line_)-1) line_[used_++] = static_cast<char>(v);
        else { oversize_ = true; used_ = 0; }
      }
    }
    if (state_ == State::Reading && !file_.available()) {
      // A truncated final line is not a valid JSONL record.
      if (used_ || oversize_) ++invalid_;
      file_.close();
      state_ = State::Done;
    }
  }
  enum class State : uint8_t { Idle, Reading, Done, Failed };
  State state() const { return state_; }
  uint16_t number() const { return number_; }
  uint32_t bytes() const { return bytes_; }
  uint32_t readBytes() const { return read_; }
  uint32_t observations(unsigned i) const { return i<3 ? count_[i] : 0; }
  uint32_t utcTagged() const { return utc_; }
  uint32_t positioned() const { return position_; }
  uint32_t invalid() const { return invalid_; }
  bool hasRssi(unsigned i) const { return i<3 && seenRssi_[i]; }
  int16_t strongest(unsigned i) const { return i<3 ? strongest_[i] : 0; }
  int16_t weakest(unsigned i) const { return i<3 ? weakest_[i] : 0; }
 private:
  void reset() {
    for (unsigned i=0;i<3;++i) {
      count_[i]=0; strongest_[i]=weakest_[i]=0; seenRssi_[i]=false;
    }
    bytes_=read_=utc_=position_=invalid_=0;
    used_=0; oversize_=false;
  }
  // Parse exact keys produced by callingcard.event.v0; don't interpret
  // arbitrary occurrence of "source" in a label string.
  void parseLine() {
    line_[used_] = 0;
    if (!strstr(line_, "\"schema\":\"callingcard.event.v0\"") ||
        !strstr(line_, "\"kind\":\"observation\"")) { ++invalid_; return; }
    const char* source = strstr(line_, "\"source\":\"");
    if (!source) { ++invalid_; return; }
    source += 10;
    int k = -1;
    if (strncmp(source, "wifi\"",5)==0) k=0;
    else if (strncmp(source, "ble\"",4)==0) k=1;
    else if (strncmp(source, "lora\"",5)==0) k=2;
    if (k<0) { ++invalid_; return; }
    ++count_[k];
    if (strstr(line_, "\"utc\":\"")) ++utc_;
    if (strstr(line_, "\"position\":{")) ++position_;
    const char* rssi = strstr(line_, "\"rssi_dbm\":");
    if (rssi) {
      char* end=nullptr;
      long value = strtol(rssi+11, &end, 10);
      if (end != rssi+11 && value>=-32768 && value<=32767) {
        int16_t level=static_cast<int16_t>(value);
        if (!seenRssi_[k] || level>strongest_[k]) strongest_[k]=level;
        if (!seenRssi_[k] || level<weakest_[k]) weakest_[k]=level;
        seenRssi_[k]=true;
      }
    }
  }
  File file_;
  State state_ = State::Idle;
  uint16_t number_ = 0;
  char line_[512]{};
  uint16_t used_ = 0;
  bool oversize_ = false;
  uint32_t bytes_=0,read_=0,utc_=0,position_=0,invalid_=0;
  uint32_t count_[3]{};
  int16_t strongest_[3]{},weakest_[3]{};
  bool seenRssi_[3]{};
};
} // namespace callingcard
