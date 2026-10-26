#pragma once
#include "DetectionEngine.h"
#include <stdint.h>
#include <string.h>
namespace callingcard {
// Bounded suppression keyed by category, rule, source, not MAC address.
// Names and GPS aren't sent over the unencrypted connection.
class DetectionCooldown {
 public:
  bool accept(DetectionType type, const char* rule, char source, uint32_t nowMs) {
    if(type==DetectionType::Unknown || !rule || !rule[0])return false;
    uint32_t hash=2166136261UL;
    for(const char* p=rule;*p;++p){hash^=static_cast<uint8_t>(*p);hash*=16777619UL;}
    hash^=(static_cast<uint32_t>(type)<<8)|static_cast<uint8_t>(source);
    const unsigned slot=hash%8;
    if(entries_[slot].set && entries_[slot].key==hash &&
       uint32_t(nowMs-entries_[slot].lastMs)<30000UL)return false;
    entries_[slot]={hash,nowMs,true};
    return true;
  }
 private:
  struct Entry {uint32_t key=0,lastMs=0;bool set=false;};
  Entry entries_[8]{};
};
} // namespace callingcard
