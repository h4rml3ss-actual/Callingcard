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
  // PR80: Specific Axon camera pairing-mode SSIDs. No generic AXON AP rule.
  bool axonSsid=false;
  if(ssid) {
    const char* prefixes[]={"AB2-","AB3-","AB4-"};
    for(unsigned i=0;i<3;++i) {
      bool equal=true;
      for(unsigned j=0;j<4;++j) {
        char c=ssid[j];
        if(!c) {equal=false;break;}
        if(c>='a'&&c<='z')c=static_cast<char>(c-('a'-'A'));
        if(c!=prefixes[i][j]){equal=false;break;}
      }
      if(equal && ssid[4])axonSsid=true;
    }
  }
  const bool axonOui=parsed &&
    ((mac[0]==0x00&&mac[1]==0x25&&mac[2]==0xDF) ||
     (mac[0]==0xE4&&mac[1]==0x05&&mac[2]==0x40));
  if(axonSsid || axonOui) {
    const DetectionEvidence ev[2]={
      {EvidenceType::WifiSsid,MatchStrength::Moderate,
        "axon-pairing-ssid","SquachWatch-CYD/docs/DETECTIONS.md"},
      {EvidenceType::WifiBssid,MatchStrength::Weak,
        "axon-vendor-oui","SquachWatch-CYD/docs/DETECTIONS.md"}
    };
    if(axonSsid && axonOui) {
      result.detection=classify(DetectionType::Axon,ev,2);
      if(result.detection.confidence==Confidence::High)
        result.detection.confidence=Confidence::Medium;
      result.ruleId="axon-pairing-ssid+vendor-oui";
    } else {
      const DetectionEvidence& evidence=axonSsid?ev[0]:ev[1];
      result.detection=classify(DetectionType::Axon,&evidence,1);
      result.ruleId=evidence.ruleId;
    }
    return result;
  }
  // Dedicated camera-vendor MA-L allocations; weak candidate only.
  struct CameraOui {uint8_t a,b,c;const char* rule;};
  static const CameraOui cameras[]={
    {0xE0,0xA7,0x00,"camera-verkada-e0a700"},
    {0x70,0x1A,0xD5,"camera-avigilon-701ad5"},
    {0x00,0x40,0x8C,"camera-axis-00408c"},
    {0xB8,0xA4,0x4F,"camera-axis-b8a44f"}
  };
  if(parsed)for(unsigned i=0;i<sizeof(cameras)/sizeof(cameras[0]);++i) {
    const CameraOui& v=cameras[i];
    if(mac[0]==v.a && mac[1]==v.b && mac[2]==v.c) {
      const DetectionEvidence ev={EvidenceType::WifiBssid,MatchStrength::Weak,
        v.rule,"SquachWatch-CYD/docs/DETECTIONS.md registry audit"};
      result.detection=classify(DetectionType::Camera,&ev,1);
      result.ruleId=v.rule;return result;
    }
  }
  // PR69: Vendor-owned allocations verified by SquachWatch's IEEE audit.
  // A Motorola Solutions or Genetec AP is NOT necessarily an ALPR device.
  // Deliberately LOW candidate confidence; no automatic high-certainty claim.
  struct AlprOui { uint8_t a,b,c; const char* rule; };
  static const AlprOui kAlpr[] = {
    {0x00,0x04,0x7D,"alpr-motorola-00047d"},
    {0x00,0x18,0x85,"alpr-motorola-001885"},
    {0x00,0x1F,0x92,"alpr-motorola-001f92"},
    {0x4C,0xCC,0x34,"alpr-motorola-4ccc34"},
    {0x00,0xBF,0x15,"alpr-genetec-00bf15"},
    {0x0C,0xBF,0x15,"alpr-genetec-0cbf15"}
  };
  if(parsed) for(unsigned i=0;i<sizeof(kAlpr)/sizeof(kAlpr[0]);++i) {
    const AlprOui& oui=kAlpr[i];
    if(mac[0]==oui.a && mac[1]==oui.b && mac[2]==oui.c) {
      const DetectionEvidence ev={EvidenceType::WifiBssid,MatchStrength::Weak,
        oui.rule,"SquachWatch-CYD/docs/DETECTIONS.md IEEE registry audit"};
      result.detection=classify(DetectionType::Alpr,&ev,1);
      result.ruleId=oui.rule;
      return result;
    }
  }
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
