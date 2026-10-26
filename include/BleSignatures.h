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
inline uint16_t shortServiceUuid(const char* text) {
  if (!text) return 0;
  const size_t n=strlen(text);
  const char* hex=text;
  if (n==36) {
    if (strncmp(text,"0000",4)!=0 ||
        strcmp(text+8,"-0000-1000-8000-00805f9b34fb")!=0) return 0;
    hex=text+4;
  } else if (n!=4) return 0;
  uint16_t value=0;
  for(int i=0;i<4;++i) {
    char c=hex[i];
    int d=c>='0'&&c<='9'?c-'0':
          c>='a'&&c<='f'?c-'a'+10:
          c>='A'&&c<='F'?c-'A'+10:-1;
    if(d<0)return 0;
    value=static_cast<uint16_t>((value<<4)|d);
  }
  return value;
}
inline BleCandidate classifyBle(const char* name, bool hasCompanyId,
                                uint16_t companyId, const char* serviceUuid) {
  BleCandidate out;
  // Company IDs and service UUIDs captured for future, corroborated rules.
  // They are intentionally not interpreted as proof on their own.
  (void)hasCompanyId; (void)companyId;
  const uint16_t uuid=shortServiceUuid(serviceUuid);
  if (uuid==0xFD5A || uuid==0xFEED || uuid==0xFEEC || uuid==0xFD5F) {
    const bool glasses=uuid==0xFD5F;
    const char* rule=glasses?"meta-fd5f-service":
      uuid==0xFD5A?"samsung-smarttag-fd5a-service":
      uuid==0xFEED?"tile-feed-service":"tile-feec-service";
    const DetectionEvidence e={EvidenceType::BleService,MatchStrength::Moderate,
      rule,"Bluetooth SIG Assigned Numbers; SquachWatch-CYD/docs/DETECTIONS.md"};
    out.detection=classify(glasses?DetectionType::Meta:DetectionType::Tracker,&e,1);
    out.ruleId=rule;
    return out;
  }
  // FEAA also identifies ordinary Eddystone beacons: no tracker claim.
  // Apple 0x004C covers many products and is not an AirTag signature.
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
