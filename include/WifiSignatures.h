#pragma once
#include "DetectionEngine.h"
#include <stdint.h>
#include <string.h>
namespace callingcard {
// Only publicly attributable high-specificity indicators; no generic ESP32,
// LiteOn, or locally administered prefixes (these create false positives).
// Source: https://github.com/skizzophrenic/SquachWatch-CYD/blob/master/docs/DETECTIONS.md
struct WifiCandidate {
  Detection detection;
  const char* ruleId = nullptr;
};
inline bool parseMacPrefix(const char* mac,uint8_t (&out)[3]) {
  if(!mac || strlen(mac)!=17)return false;
  for(int i=0;i<6;i++) {
    int p=i<3?i*3:0; (void)p;
    const int a=i*3;
    const char h=mac[a], l=mac[a+1];
    const auto hex=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; };
    if(hex(h)<0||hex(l)<0||(i<5&&mac[a+2]!=':'))return false;
    if(i<3)out[i]=static_cast<uint8_t>((hex(h)<<4)|hex(l));
  }
  return true;
}
inline bool flockSsid(const char* ssid) {
  if(!ssid) return false;
  const char prefix[]="flock-";
  for(size_t i=0;i<6;++i) {
    char c=ssid[i]; if(!c)return false;
    if(c>='A'&&c<='Z')c=static_cast<char>(c+('a'-'A'));
    if(c!=prefix[i])return false;
  }
  return true;
}
// Classification does NOT confirm equipment type or record exact BSSID.
// A prefix belongs to a vendor allocation, not proof of a physical camera.
inline WifiCandidate classifyWifi(const char* ssid,const char* bssid) {
  WifiCandidate result;
  uint8_t mac[3]={};
  const bool parsed=parseMacPrefix(bssid,mac);
  const bool vendor=parsed && mac[0]==0xB4 && mac[1]==0x1E && mac[2]==0x52;
  const bool name=flockSsid(ssid);
  if(!vendor&&!name)return result;
  // Deliberately score a composite WiFi-only signal at medium maximum:
  // SSID and BSSID are both spoofable and from one transmitter.
  DetectionEvidence ev[2]={
    {EvidenceType::WifiBssid,MatchStrength::Moderate,"flock-ieee-b41e52","SquachWatch-CYD/docs/DETECTIONS.md"},
    {EvidenceType::WifiSsid,MatchStrength::Weak,"flock-ssid-prefix","SquachWatch-CYD/docs/DETECTIONS.md"}
  };
  if(vendor && name) {
    result.detection=classify(DetectionType::Flock,ev,2);
    if(result.detection.confidence==Confidence::High) result.detection.confidence=Confidence::Medium;
    result.ruleId="flock-ieee-b41e52+flock-ssid-prefix";
  } else if(vendor) {
    result.detection=classify(DetectionType::Flock,ev,1);
    result.ruleId=ev[0].ruleId;
  } else {
    result.detection=classify(DetectionType::Flock,ev+1,1);
    result.ruleId=ev[1].ruleId;
  }
  return result;
}
} // namespace callingcard
