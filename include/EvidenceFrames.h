#pragma once
#include "Observation.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace callingcard {
// Extension to PR74 H/P/T: bounded X fragments, 8 bytes per fragment.
// X,<seq>,<I|R>,<index>,<total>,<ASCII> (19 chars max).
struct EvidenceFrames {
  char fragments[10][20] = {};
  uint8_t count=0;
};
inline bool evidenceCharacter(char c) {
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
         (c>='0'&&c<='9')||c==':'||c=='-'||c=='_'||c=='.';
}
inline bool appendEvidence(EvidenceFrames& frames, uint16_t seq,
                           char field, const char* data, size_t capacity) {
  if(!data || !data[0])return true;
  const size_t len=strnlen(data,capacity);
  if(len==capacity || len>48)return false;
  for(size_t i=0;i<len;++i)if(!evidenceCharacter(data[i]))return false;
  const size_t pieces=(len+7)/8;
  if(pieces>9 || frames.count+pieces>10)return false;
  for(size_t i=0;i<pieces;++i) {
    char chunk[9]={};
    const size_t offset=i*8;
    const size_t take=(len-offset<8)?len-offset:8;
    memcpy(chunk,data+offset,take);
    const int written=snprintf(frames.fragments[frames.count],20,
       "X,%u,%c,%u,%u,%s",(unsigned)seq,field,(unsigned)i,
       (unsigned)pieces,chunk);
    if(written<0 || written>=20)return false;
    ++frames.count;
  }
  return true;
}
inline bool formatEvidenceFrames(const Observation& observation,
                                uint16_t seq, EvidenceFrames& out) {
  EvidenceFrames next{};
  const char* identifier=observation.kind==Kind::WifiNetwork ?
       observation.wifiBssid : observation.kind==Kind::BleAdvertisement ?
       observation.bleAddress : nullptr;
  if(!appendEvidence(next,seq,'I',identifier,18))return false;
  if(!appendEvidence(next,seq,'R',observation.detectionRule,48))return false;
  out=next;
  return true;
}
} // namespace callingcard
