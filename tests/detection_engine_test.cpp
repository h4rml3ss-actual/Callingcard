#include "DetectionEngine.h"
#include <cassert>
#include <cstring>
using namespace callingcard;
int main() {
  assert(!classify(DetectionType::Flock,nullptr,0).valid);
  DetectionEvidence noSource[]={{EvidenceType::WifiSsid,MatchStrength::Strong,"ssid.foo",nullptr}};
  assert(!classify(DetectionType::Flock,noSource,1).valid);
  DetectionEvidence weak[]={{EvidenceType::WifiBssid,MatchStrength::Weak,"generic-chip","fixture:weak-mac"}};
  auto one=classify(DetectionType::Flock,weak,1);
  assert(one.valid && one.confidence==Confidence::Low && one.independentKinds==1);
  DetectionEvidence sameKind[]={
    {EvidenceType::WifiSsid,MatchStrength::Moderate,"rule1","fixture:1"},
    {EvidenceType::WifiSsid,MatchStrength::Strong,"rule2","fixture:2"}
  };
  auto repeated=classify(DetectionType::Alpr,sameKind,2);
  assert(repeated.independentKinds==1 && repeated.confidence==Confidence::Medium);
  DetectionEvidence independent[]={
    {EvidenceType::WifiSsid,MatchStrength::Strong,"rule3","fixture:3"},
    {EvidenceType::BleManufacturer,MatchStrength::Moderate,"rule4","fixture:4"}
  };
  auto two=classify(DetectionType::Axon,independent,2);
  assert(two.valid && two.independentKinds==2 && two.confidence==Confidence::High);
  assert(std::strcmp(detectionTypeName(DetectionType::Skimmer),"SKIMMER")==0);
  assert(std::strcmp(confidenceName(Confidence::None),"NONE")==0);
}
