#pragma once
#include <Arduino.h>
#include "Observation.h"

namespace callingcard {
// Counts emitted observations during a session, including while paused.
// Saved counts are only incremented after a full JSONL mirror write to SD.
class SessionStats {
 public:
  void begin(uint32_t now, uint32_t pipelineDropped, uint32_t bleDropped) {
    *this = SessionStats{};
    startedMs_ = now;
    pipelineBase_ = pipelineDropped;
    bleBase_ = bleDropped;
    active_ = true;
  }
  void finish(uint32_t now, uint32_t pipelineDropped, uint32_t bleDropped) {
    endedMs_ = now;
    pipelineLoss_ = pipelineDropped - pipelineBase_;
    bleLoss_ = bleDropped - bleBase_;
    active_ = false;
  }
  void onEvent(const Observation& o, bool saved) {
    if (!active_) return;
    ++processed_;
    if (saved) ++saved_;
    else ++unsaved_;
    const unsigned k = static_cast<unsigned>(o.kind);
    if (k < 3) {
      ++bySource_[k];
      if (saved) ++savedSource_[k];
      if (!seenRssi_[k] || o.rssiDbm > strongest_[k]) strongest_[k] = o.rssiDbm;
      if (!seenRssi_[k] || o.rssiDbm < weakest_[k]) weakest_[k] = o.rssiDbm;
      seenRssi_[k] = true;
    }
    if (o.hasUtc) ++utcTagged_;
    if (o.hasPosition) ++positionTagged_;
    if (o.kind == Kind::WifiNetwork) {
      // Fixed-size approximate unique SSID count; no device identities stored.
      uint32_t hash = 2166136261u;
      for (const char* p = o.label; *p; ++p) {
        hash = (hash ^ static_cast<uint8_t>(*p)) * 16777619u;
      }
      bool seen = false;
      for (uint8_t i = 0; i < ssidUsed_; ++i) if (ssids_[i] == hash) seen = true;
      if (!seen && ssidUsed_ < kSsidCapacity) ssids_[ssidUsed_++] = hash;
      if (!seen && ssidUsed_ == kSsidCapacity) ssidLimit_ = true;
    }
  }
  void print(Print& out, uint32_t now, uint32_t pipelineDropped, uint32_t bleDropped, uint32_t bytes, const char* path) const {
    const uint32_t elapsed = (active_ ? now : endedMs_) - startedMs_;
    out.printf("SESSION SUMMARY path=%s duration_ms=%lu processed=%lu saved=%lu unsaved=%lu bytes=%lu wifi=%lu ble=%lu lora=%lu wifi_saved=%lu ble_saved=%lu lora_saved=%lu ssid_hashes=%u ssid_limit=%u utc_tagged=%lu position_tagged=%lu pipeline_lost=%lu ble_prequeue_lost=%lu\n",
      path, (unsigned long)elapsed, (unsigned long)processed_, (unsigned long)saved_,
      (unsigned long)unsaved_, (unsigned long)bytes,
      (unsigned long)bySource_[0], (unsigned long)bySource_[1], (unsigned long)bySource_[2],
      (unsigned long)savedSource_[0], (unsigned long)savedSource_[1], (unsigned long)savedSource_[2],
      ssidUsed_, ssidLimit_, (unsigned long)utcTagged_, (unsigned long)positionTagged_,
      (unsigned long)(active_ ? pipelineDropped-pipelineBase_ : pipelineLoss_),
      (unsigned long)(active_ ? bleDropped-bleBase_ : bleLoss_));
    for (unsigned i=0;i<3;++i)
      if (seenRssi_[i]) out.printf("SESSION RSSI source=%s strongest=%d weakest=%d\n",
        i==0?"wifi":i==1?"ble":"lora", strongest_[i], weakest_[i]);
  }
  bool hasSession() const { return started_; }
  void markStarted() { started_ = true; }
  uint32_t count(unsigned k) const { return k < 3 ? bySource_[k] : 0; }
  uint32_t saved() const { return saved_; }
  uint32_t elapsed(uint32_t now) const { return (active_ ? now : endedMs_) - startedMs_; }
 private:
  static constexpr uint8_t kSsidCapacity = 32;
  uint32_t ssids_[kSsidCapacity]{};
  uint8_t ssidUsed_ = 0;
  bool ssidLimit_ = false;
  bool started_ = false;
  bool active_ = false;
  uint32_t startedMs_ = 0, endedMs_ = 0, pipelineBase_ = 0, bleBase_ = 0;
  uint32_t pipelineLoss_ = 0, bleLoss_ = 0;
  uint32_t processed_ = 0, saved_ = 0, unsaved_ = 0;
  uint32_t bySource_[3]{}, savedSource_[3]{};
  int16_t strongest_[3]{}, weakest_[3]{};
  bool seenRssi_[3]{};
  uint32_t utcTagged_ = 0, positionTagged_ = 0;
};
} // namespace callingcard
