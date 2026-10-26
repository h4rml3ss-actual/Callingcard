#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace callingcard {
// PR63: classification core only. No radio capture or vendor signature claims.
enum class DetectionType : uint8_t {
  Unknown, Flock, Alpr, Axon, Camera, Tracker, Meta, Skimmer
};
enum class EvidenceType : uint8_t {
  WifiSsid, WifiBssid, BleName, BleManufacturer, BleService, WifiManagement
};
enum class Confidence : uint8_t { None, Low, Medium, High };
enum class MatchStrength : uint8_t { Weak=1, Moderate=2, Strong=3 };

struct DetectionEvidence {
  EvidenceType kind;
  MatchStrength strength;
  const char* ruleId;      // stable attribution from vetted rule registry
  const char* source;      // provenance: e.g. research URL or local test fixture ID
};

struct Detection {
  DetectionType type = DetectionType::Unknown;
  Confidence confidence = Confidence::None;
  uint8_t evidenceCount = 0;
  uint8_t independentKinds = 0;
  bool valid = false;
};

inline const char* detectionTypeName(DetectionType v) {
  switch(v) {
    case DetectionType::Flock: return "FLOCK";
    case DetectionType::Alpr: return "ALPR";
    case DetectionType::Axon: return "AXON";
    case DetectionType::Camera: return "CAMERA";
    case DetectionType::Tracker: return "TRACKER";
    case DetectionType::Meta: return "META";
    case DetectionType::Skimmer: return "SKIMMER";
    default: return "UNKNOWN";
  }
}
inline const char* confidenceName(Confidence v) {
  switch(v) {
    case Confidence::Low: return "LOW";
    case Confidence::Medium: return "MEDIUM";
    case Confidence::High: return "HIGH";
    default: return "NONE";
  }
}

// One observation can match several rules, but repeated evidence from a single
// source/kind must not inflate confidence. Callers supply already-matched rule
// evidence; this function never claims a device vendor from a MAC alone.
inline Detection classify(DetectionType candidate, const DetectionEvidence* evidence,
                          size_t count) {
  Detection out;
  if(candidate==DetectionType::Unknown || evidence==nullptr || count==0) return out;
  uint32_t seenKinds=0;
  uint8_t strongest=0;
  uint8_t unique=0;
  for(size_t i=0;i<count;++i) {
    const DetectionEvidence& e=evidence[i];
    if(e.ruleId==nullptr || !e.ruleId[0] || e.source==nullptr || !e.source[0])
      continue; // evidence without provenance is inadmissible
    const unsigned kind=static_cast<unsigned>(e.kind);
    if(kind>5) continue;
    const uint32_t bit=1UL<<kind;
    if(seenKinds & bit) continue; // same modality is not independent evidence
    seenKinds|=bit;
    ++unique;
    const uint8_t s=static_cast<uint8_t>(e.strength);
    if(s>=1 && s<=3 && s>strongest) strongest=s;
  }
  if(!unique || !strongest) return out;
  out.valid=true;
  out.type=candidate;
  out.evidenceCount=static_cast<uint8_t>(count>255?255:count);
  out.independentKinds=unique;
  // Even a strong single signal is at most MEDIUM. HIGH requires at least
  // two independent observation modalities, including one strong rule.
  out.confidence=(unique>=2 && strongest>=3) ? Confidence::High :
    (strongest>=2 ? Confidence::Medium : Confidence::Low);
  return out;
}
} // namespace callingcard
