#pragma once
#include "Observation.h"
#include <stdint.h>
#include <string.h>
namespace callingcard {
// Session-local exact-address correlation; never infer shared physical identity.
class ObservationCorrelator {
 public:
  static constexpr unsigned Capacity=32;
  struct Summary {
    bool matched=false;
    uint16_t sightings=0;
    int16_t strongestRssi=0;
    int16_t latestRssi=0;
    uint32_t firstUptimeMs=0;
    uint32_t lastUptimeMs=0;
  };
  Summary observe(const Observation& o) {
    Summary none{};
    if(!o.hasDetection)return none;
    const char* addr=o.kind==Kind::WifiNetwork?o.wifiBssid:
      o.kind==Kind::BleAdvertisement?o.bleAddress:nullptr;
    if(!addr || strlen(addr)!=17)return none;
    for(unsigned i=0;i<17;++i) {
      const char c=addr[i];
      if(i%3==2) { if(c!=':')return none; }
      else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||
                 (c>='A'&&c<='F')))return none;
    }
    Entry* match=nullptr;
    Entry* freeSlot=nullptr;
    Entry* oldest=&entries_[0];
    for(unsigned i=0;i<Capacity;++i) {
      Entry& e=entries_[i];
      if(!e.valid){if(!freeSlot)freeSlot=&e;continue;}
      // Millis wrap-safe comparison for bounded session time spans.
      if(static_cast<int32_t>(e.last-o.uptimeMs)>static_cast<int32_t>(oldest->last-o.uptimeMs))
        oldest=&e;
      if(e.kind==o.kind && e.typeEquals(o.detectionType) &&
         equalsIgnoreCase(e.address,addr)){match=&e;break;}
    }
    Entry* target=match?match:freeSlot?freeSlot:oldest;
    if(!match) {
      *target=Entry{};
      target->valid=true;target->kind=o.kind;
      memcpy(target->address,addr,17);target->address[17]=0;
      copy(target->type,sizeof(target->type),o.detectionType);
      target->first=o.uptimeMs;target->strongest=o.rssiDbm;
    }
    if(target->count<65535)++target->count;
    if(o.rssiDbm>target->strongest)target->strongest=o.rssiDbm;
    target->latest=o.rssiDbm;target->last=o.uptimeMs;
    Summary summary{};
    summary.matched=true;summary.sightings=target->count;
    summary.strongestRssi=target->strongest;summary.latestRssi=target->latest;
    summary.firstUptimeMs=target->first;summary.lastUptimeMs=target->last;
    return summary;
  }
  void reset(){for(unsigned i=0;i<Capacity;++i)entries_[i]=Entry{};}
 private:
  struct Entry {
    bool valid=false;Kind kind=Kind::WifiNetwork;char address[18]={};
    char type[12]={};uint16_t count=0;int16_t strongest=0,latest=0;
    uint32_t first=0,last=0;
    bool typeEquals(const char* value) const {return strcmp(type,value)==0;}
  };
  Entry entries_[Capacity]={};
  static bool equalsIgnoreCase(const char* a,const char* b) {
    for(unsigned i=0;i<17;++i) {
      char x=a[i],y=b[i];
      if(x>='a'&&x<='f')x-=32;
      if(y>='a'&&y<='f')y-=32;
      if(x!=y)return false;
    }
    return true;
  }
  static void copy(char* dest,unsigned cap,const char* src) {
    unsigned i=0;for(;i+1<cap && src[i];++i)dest[i]=src[i];
    dest[i]=0;
  }
};
} // namespace callingcard
