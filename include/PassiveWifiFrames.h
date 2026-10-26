#pragma once
#include <stdint.h>
namespace callingcard {
// Passive monitor never stores addresses, SSIDs, frame bodies, or payloads.
enum class ManagementKind : uint8_t { Other, Beacon, ProbeRequest, ProbeResponse, Deauth, Disassoc };
inline ManagementKind managementKind(const uint8_t* bytes, unsigned length) {
  if(!bytes || length<2)return ManagementKind::Other;
  // Frame control: protocol version must be 0, type must be management (00).
  if((bytes[0]&0x0f)!=0)return ManagementKind::Other;
  switch((bytes[0]>>4)&0x0f) {
    case 8:return ManagementKind::Beacon;
    case 4:return ManagementKind::ProbeRequest;
    case 5:return ManagementKind::ProbeResponse;
    case 12:return ManagementKind::Deauth;
    case 10:return ManagementKind::Disassoc;
    default:return ManagementKind::Other;
  }
}
} // namespace callingcard
