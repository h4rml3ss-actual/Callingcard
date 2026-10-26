#include "WifiSignatures.h"
#include <cassert>
using namespace callingcard;
int main(){
  auto none=classifyWifi("home","24:0A:C4:12:34:56"); // generic ESP32 OUI
  assert(!none.detection.valid);
  assert(classifyWifi("FLOCK-CAMERA","garbage").detection.valid); // SSID alone is a weak indicator
  auto oui=classifyWifi("nondescript","B4:1E:52:11:22:33");
  assert(oui.detection.valid && oui.detection.type==DetectionType::Flock);
  assert(oui.detection.confidence==Confidence::Medium);
  auto ssid=classifyWifi("Flock-example","00:11:22:33:44:55");
  assert(ssid.detection.valid && ssid.detection.confidence==Confidence::Low);
  auto both=classifyWifi("flock-unit","b4:1e:52:01:02:03");
  assert(both.detection.valid && both.detection.confidence==Confidence::Medium);
  assert(!classifyWifi("flocking birds","00:11:22:33:44:55").detection.valid);
  assert(!classifyWifi("<hidden>","02:11:22:33:44:55").detection.valid);
  assert(!classifyWifi("test","b4:1e:52:broken").detection.valid);
  const char* alpr[]={
    "00:04:7D:11:22:33","00:18:85:11:22:33",
    "00:1F:92:11:22:33","4C:CC:34:11:22:33",
    "00:BF:15:11:22:33","0C:BF:15:11:22:33"
  };
  for(const char* mac:alpr) {
    const auto match=classifyWifi("ordinary-network",mac);
    assert(match.detection.valid && match.detection.type==DetectionType::Alpr);
    assert(match.detection.confidence==Confidence::Low);
    assert(match.ruleId!=nullptr);
  }
  // Sonos OUI was historically misidentified as Motorola/Vigilant.
  assert(!classifyWifi("ordinary-network","00:0E:58:11:22:33").detection.valid);
  // Shared chipset and random/local OUIs do not imply ALPR.
  assert(!classifyWifi("ordinary-network","24:0A:C4:00:11:22").detection.valid);
  assert(!classifyWifi("ordinary-network","02:04:7D:11:22:33").detection.valid);
  assert(!classifyWifi("ordinary-network","4C:CC:34:XX:22:33").detection.valid);

  const char* axonNames[]={"AB2-1234","ab3-TEST","AB4-9012"};
  for(const char* name:axonNames) {
    auto found=classifyWifi(name,"00:11:22:33:44:55");
    assert(found.detection.valid && found.detection.type==DetectionType::Axon);
    assert(found.detection.confidence!=Confidence::High);
  }
  assert(!classifyWifi("AB2","00:11:22:33:44:55").detection.valid);
  assert(!classifyWifi("AB2-","00:11:22:33:44:55").detection.valid);
  assert(!classifyWifi("AXONOMY","00:11:22:33:44:55").detection.valid);
  for(const char* mac:{"00:25:DF:11:22:33","E4:05:40:11:22:33"}) {
    auto found=classifyWifi("ordinary",mac);
    assert(found.detection.valid && found.detection.type==DetectionType::Axon);
    assert(found.detection.confidence==Confidence::Low);
  }
  const char* cameraMacs[]={"E0:A7:00:11:22:33","70:1A:D5:11:22:33",
                            "00:40:8C:11:22:33","B8:A4:4F:11:22:33"};
  for(const char* mac:cameraMacs) {
    auto found=classifyWifi("ordinary",mac);
    assert(found.detection.valid && found.detection.type==DetectionType::Camera);
    assert(found.detection.confidence==Confidence::Low);
  }
  assert(!classifyWifi("ordinary","24:0A:C4:00:11:22").detection.valid);
  assert(!classifyWifi("ordinary","02:A7:00:11:22:33").detection.valid);
}
