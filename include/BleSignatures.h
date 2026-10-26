#pragma once
#include "DetectionEngine.h"
#include <stdint.h>
#include <string.h>
namespace callingcard {
struct BleCandidate { Detection detection; const char* ruleId=nullptr; };
// A name is controlled by the advertiser and can be spoofed. Vendor company
// ID alone identifies a registration, NOT a particular tracker or camera.
inline bool startsWithToken(const char* name, const char* token) {
  if(!name||!token)return false;
  const size_t n=strlen(token);
  if(strncmp(name,token,n)!=0)return false;
  const char c=name[n];
  return c==0 || c==' ' || c=='-' || c=='_' || c=='(';
}
inline BleCandidate classifyBle(const char* name, bool hasCompanyId,
                                uint16_t companyId, const char* serviceUuid) {
  BleCandidate out;
  // Company IDs and service UUIDs captured for future, corroborated rules.
  // They are intentionally not interpreted as proof on their own.
  (void)hasCompanyId; (void)companyId; (void)serviceUuid;
  DetectionType type=DetectionType::Unknown;
  const char* rule=nullptr;
  if(startsWithToken(name,"Tile") || startsWithToken(name,"SmartTag")) {
    type=DetectionType::Tracker; rule="ble-advertised-tracker-name";
  } else if(startsWithToken(name,"AXON")) {
    type=DetectionType::Axon; rule="ble-advertised-axon-name";
  } else if(startsWithToken(name,"GoPro")) {
    type=DetectionType::Camera; rule="ble-advertised-gopro-name";
  }
  if(type==DetectionType::Unknown)return out;
  const DetectionEvidence e={EvidenceType::BleName,MatchStrength::Weak,
    rule,"Callingcard PR65 observational name-only heuristic"};
  out.detection=classify(type,&e,1);
  out.ruleId=rule;
  return out;
}
} // namespace callingcard
