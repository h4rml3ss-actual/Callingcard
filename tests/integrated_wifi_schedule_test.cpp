#include "IntegratedWifiSchedule.h"
#include "PassiveWifiFrames.h"
#include <cassert>
using namespace callingcard;
int main() {
  IntegratedWifiSchedule scheduler{};
  assert(!scheduler.shouldStartPassive());
  scheduler.onActiveWifiFinished();
  assert(scheduler.shouldStartPassive());
  scheduler.onPassiveFinished();
  assert(!scheduler.shouldStartPassive());
  scheduler.onActiveWifiFinished();
  scheduler.reset();
  assert(!scheduler.shouldStartPassive());
  const uint8_t beacon[]={0x80,0x00};
  const uint8_t probe[]={0x40,0x00};
  const uint8_t deauth[]={0xc0,0x00};
  const uint8_t data[]={0x08,0x00};
  assert(managementKind(beacon,2)==ManagementKind::Beacon);
  assert(managementKind(probe,2)==ManagementKind::ProbeRequest);
  assert(managementKind(deauth,2)==ManagementKind::Deauth);
  assert(managementKind(data,2)==ManagementKind::Other);
}
