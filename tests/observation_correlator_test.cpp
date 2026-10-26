#include "ObservationCorrelator.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace callingcard;
static Observation sighting(Kind kind,const char* addr,const char* type,
                            uint32_t ms,int rssi) {
  Observation o{};
  o.kind=kind;o.hasDetection=true;o.uptimeMs=ms;o.rssiDbm=rssi;
  std::strcpy(o.detectionType,type);
  std::strcpy(kind==Kind::WifiNetwork?o.wifiBssid:o.bleAddress,addr);
  return o;
}
int main() {
  ObservationCorrelator tracker{};
  const char* a="AA:BB:CC:DD:EE:01";
  auto first=tracker.observe(sighting(Kind::WifiNetwork,a,"FLOCK",100,-80));
  assert(first.matched&&first.sightings==1&&first.strongestRssi==-80);
  auto second=tracker.observe(sighting(Kind::WifiNetwork,"aa:bb:cc:dd:ee:01","FLOCK",200,-61));
  assert(second.sightings==2&&second.firstUptimeMs==100&&second.strongestRssi==-61);
  auto weaker=tracker.observe(sighting(Kind::WifiNetwork,a,"FLOCK",300,-90));
  assert(weaker.sightings==3&&weaker.strongestRssi==-61&&weaker.latestRssi==-90);
  assert(tracker.observe(sighting(Kind::BleAdvertisement,a,"FLOCK",400,-55)).sightings==1);
  assert(tracker.observe(sighting(Kind::WifiNetwork,a,"ALPR",500,-55)).sightings==1);
  Observation noId=sighting(Kind::WifiNetwork,a,"FLOCK",600,-30);
  noId.wifiBssid[0]=0;
  assert(!tracker.observe(noId).matched);
  Observation unclassified=sighting(Kind::WifiNetwork,a,"FLOCK",700,-30);
  unclassified.hasDetection=false;
  assert(!tracker.observe(unclassified).matched);
  for(unsigned i=0;i<40;++i) {
    char addr[18];std::snprintf(addr,sizeof(addr),"AA:BB:CC:DD:EE:%02X",i+5);
    auto entry=tracker.observe(sighting(Kind::WifiNetwork,addr,"FLOCK",800+i,-70));
    assert(entry.sightings==1);
  }
  tracker.reset();
  assert(tracker.observe(sighting(Kind::WifiNetwork,a,"FLOCK",900,-60)).sightings==1);
}
