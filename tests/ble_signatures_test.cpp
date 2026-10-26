#include "BleSignatures.h"
#include <cassert>
using namespace callingcard;
int main() {
  assert(!classifyBle(nullptr,false,0,nullptr).detection.valid);
  assert(!classifyBle("iPhone",true,0x004c,"180F").detection.valid);
  assert(!classifyBle("random",true,0x004c,"180F").detection.valid);
  assert(!classifyBle("AirTag",true,0x004c,nullptr).detection.valid);
  assert(!classifyBle("Tilefish",false,0,nullptr).detection.valid);
  assert(!classifyBle("AXONOMY",false,0,nullptr).detection.valid);
  const auto tile=classifyBle("Tile Pro",false,0,nullptr);
  assert(tile.detection.valid && tile.detection.type==DetectionType::Tracker);
  assert(tile.detection.confidence==Confidence::Low && tile.detection.independentKinds==1);
  const auto smarttag=classifyBle("SmartTag-1",true,0x0075,"abcd");
  assert(smarttag.detection.valid && smarttag.detection.type==DetectionType::Tracker);
  assert(smarttag.detection.confidence==Confidence::Low);
  const auto axon=classifyBle("AXON Body",false,0,nullptr);
  assert(axon.detection.valid && axon.detection.type==DetectionType::Axon);
  assert(axon.detection.confidence==Confidence::Low);
  const auto camera=classifyBle("GoPro-001",false,0,nullptr);
  assert(camera.detection.valid && camera.detection.type==DetectionType::Camera);
  assert(camera.detection.confidence==Confidence::Low);
}
