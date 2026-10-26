#pragma once
#include "Observation.h"
#include <stdio.h>
#include <stdint.h>
namespace callingcard {
// Fixed-size, opt-in v2 fragments. One pending event at a time.
// Each fragment fits the default 20-byte BLE ATT notification payload.
struct GeoFrames {
  char header[20] = {};
  char position[20] = {};
  char timestamp[20] = {};
  bool hasPosition=false;
  bool hasTimestamp=false;
};
inline bool formatGeoFrames(const Observation& o, uint16_t seq, GeoFrames& out) {
  if(!o.hasDetection || (o.kind!=Kind::WifiNetwork && o.kind!=Kind::BleAdvertisement))
    return false;
  const char source=o.kind==Kind::WifiNetwork?'W':'B';
  const char* types[]={"FLOCK","ALPR","AXON","CAMERA","TRACKER","META","SKIMMER"};
  char code='?';
  for(unsigned i=0;i<7;++i) {
    const char* a=types[i]; const char* b=o.detectionType;
    while(*a && *b && *a==*b){++a;++b;}
    if(!*a && !*b){code=static_cast<char>('0'+i);break;}
  }
  if(code=='?')return false;
  const char confidence=o.detectionConfidence[0];
  if(confidence!='L' && confidence!='M' && confidence!='H')return false;
  // H,SSS,T,C,R,S ; sequence never reused within a short session window.
  if(snprintf(out.header,sizeof(out.header),"H,%u,%c,%c,%d,%c",
              static_cast<unsigned>(seq),code,confidence,o.rssiDbm,source)>=
       static_cast<int>(sizeof(out.header)))return false;
  if(o.hasPosition && o.latitude>=-90 && o.latitude<=90 &&
     o.longitude>=-180 && o.longitude<=180) {
    // 1e-4 degree approx 11m latitude, intentionally quantized to fit ATT.
    const long lat=static_cast<long>(o.latitude*10000.0);
    const long lon=static_cast<long>(o.longitude*10000.0);
    out.hasPosition=snprintf(out.position,sizeof(out.position),"P,%u,%ld,%ld",
      static_cast<unsigned>(seq),lat,lon)<static_cast<int>(sizeof(out.position));
  }
  if(o.hasUtc && o.utc[0]) {
    // UTC is transmitted as 10-digit seconds-since-2020? No fabricated epoch:
    // ISO time HHMMSS fragment retained; full UTC stays on SD JSONL.
    out.hasTimestamp=snprintf(out.timestamp,sizeof(out.timestamp),"T,%u,%.6s",
      static_cast<unsigned>(seq),o.utc+11)<static_cast<int>(sizeof(out.timestamp));
  }
  return true;
}
} // namespace callingcard
