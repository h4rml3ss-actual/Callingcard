#include "GeoFrames.h"
#include <cassert>
#include <cstring>
using namespace callingcard;
int main() {
  GeoFrames out{};
  Observation empty{};
  assert(!formatGeoFrames(empty,1,out));
  Observation o{};
  o.kind=Kind::WifiNetwork; o.hasDetection=true;
  std::strcpy(o.detectionType,"FLOCK");
  std::strcpy(o.detectionConfidence,"LOW");
  o.rssiDbm=-58;
  o.hasPosition=true;o.latitude=42.3601;o.longitude=-71.0589;
  o.hasUtc=true;std::strcpy(o.utc,"2026-10-26T20:15:30Z");
  assert(formatGeoFrames(o,1,out));
  assert(std::strcmp(out.header,"H,1,0,L,-58,W")==0);
  assert(out.hasPosition && std::strlen(out.position)<20);
  assert(out.hasTimestamp && std::strcmp(out.timestamp,"T,1,261026201530")==0);
  Observation stale=o;stale.hasPosition=false;stale.hasUtc=false;
  GeoFrames next{};
  assert(formatGeoFrames(stale,2,next));
  assert(!next.hasPosition && !next.hasTimestamp);
  Observation invalid=o;std::strcpy(invalid.detectionType,"UNRECOGNIZED");
  GeoFrames bad{};
  assert(!formatGeoFrames(invalid,3,bad));
}
