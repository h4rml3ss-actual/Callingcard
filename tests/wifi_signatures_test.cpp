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

}
