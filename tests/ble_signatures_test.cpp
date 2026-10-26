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
  assert(shortServiceUuid("FD5A")==0xFD5A);
  assert(shortServiceUuid("0000fd5f-0000-1000-8000-00805f9b34fb")==0xFD5F);
  assert(shortServiceUuid("garbage")==0);
  assert(shortServiceUuid("0000fd5f-0000-1000-8000-00805f9b34fc")==0);
  auto samsung=classifyBle("<unnamed BLE>",false,0,"fd5a");
  assert(samsung.detection.valid && samsung.detection.type==DetectionType::Tracker);
  assert(samsung.detection.confidence==Confidence::Medium);
  auto tileService=classifyBle("unknown",false,0,"0000feed-0000-1000-8000-00805f9b34fb");
  assert(tileService.detection.valid && tileService.detection.type==DetectionType::Tracker);
  assert(tileService.detection.confidence==Confidence::Medium);
  auto meta=classifyBle("unknown",false,0,"FD5F");
  assert(meta.detection.valid && meta.detection.type==DetectionType::Meta);
  assert(meta.detection.confidence==Confidence::Medium);
  assert(!classifyBle("unknown",true,0x004c,"FEAA").detection.valid);
  assert(!classifyBle("unknown",true,0x01ab,nullptr).detection.valid);

}
