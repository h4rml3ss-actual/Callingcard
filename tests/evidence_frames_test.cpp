#include "EvidenceFrames.h"
#include <cassert>
#include <cstring>
using namespace callingcard;
int main() {
  Observation a{};
  a.kind=Kind::WifiNetwork;
  std::strcpy(a.wifiBssid,"B4:1E:52:01:02:03");
  std::strcpy(a.detectionRule,"flock-ssid-prefix");
  EvidenceFrames frames{};
  assert(formatEvidenceFrames(a,15,frames));
  assert(frames.count==6);
  for(unsigned i=0;i<frames.count;++i) assert(std::strlen(frames.fragments[i])<20);
  assert(std::strncmp(frames.fragments[0],"X,15,I,0,3,",11)==0);
  Observation b=a;
  std::strcpy(b.detectionRule,"bad,comma");
  EvidenceFrames failed{};
  assert(!formatEvidenceFrames(b,1,failed));
  Observation c{};
  c.kind=Kind::BleAdvertisement;
  std::strcpy(c.bleAddress,"AA:BB:CC:DD:EE:FF");
  EvidenceFrames ble{};
  assert(formatEvidenceFrames(c,999,ble));
  assert(ble.count==3);

}
