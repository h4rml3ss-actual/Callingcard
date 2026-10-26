#include "PassiveWifiFrames.h"
#include <cassert>
using namespace callingcard;
int main() {
  const uint8_t beacon[]={0x80,0x00};
  const uint8_t probeRequest[]={0x40,0x00};
  const uint8_t probeResponse[]={0x50,0x00};
  const uint8_t deauth[]={0xc0,0x00};
  const uint8_t disassoc[]={0xa0,0x00};
  const uint8_t data[]={0x08,0x00};
  const uint8_t invalidVersion[]={0x81,0x00};
  assert(managementKind(beacon,2)==ManagementKind::Beacon);
  assert(managementKind(probeRequest,2)==ManagementKind::ProbeRequest);
  assert(managementKind(probeResponse,2)==ManagementKind::ProbeResponse);
  assert(managementKind(deauth,2)==ManagementKind::Deauth);
  assert(managementKind(disassoc,2)==ManagementKind::Disassoc);
  assert(managementKind(data,2)==ManagementKind::Other);
  assert(managementKind(invalidVersion,2)==ManagementKind::Other);
  assert(managementKind(beacon,1)==ManagementKind::Other);
  assert(managementKind(nullptr,0)==ManagementKind::Other);
}
