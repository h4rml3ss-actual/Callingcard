#include "EventPipeline.h"

namespace callingcard {
bool EventPipeline::enqueue(const Observation& observation) {
  if (count_ == kCapacity) {
    ++dropped_;
    return false;
  }
  queue_[tail_] = observation;
  tail_ = static_cast<uint8_t>((tail_ + 1) % kCapacity);
  ++count_;
  return true;
}

void EventPipeline::printEscaped(Print& out, const char* data) {
  // JSON string escaping, including control bytes and quotation marks.
  static constexpr char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < sizeof(Observation::label) && data[i]; ++i) {
    const uint8_t c = static_cast<uint8_t>(data[i]);
    if (c == '"' || c == '\\') {
      out.write('\\');
      out.write(c);
    } else if (c < 0x20) {
      out.print("\\u00");
      out.write(hex[c >> 4]);
      out.write(hex[c & 0x0f]);
    } else {
      out.write(c);
    }
  }
}

void EventPipeline::printEvent(Print& out, const Observation& o) {
  // UTC and coordinates are intentionally absent until a valid time/fix
  // source can be associated with the specific observation.
  out.print(F("{\"schema\":\"callingcard.event.v0\",\"seq\":"));
  out.print(++sequence_);
  out.print(F(",\"kind\":\"observation\",\"source\":\""));
  out.print(kindName(o.kind));
  out.print(F("\",\"uptime_ms\":"));
  out.print(o.uptimeMs);
  out.print(F(",\"rssi_dbm\":"));
  out.print(o.rssiDbm);
  out.print(F(",\"channel\":"));
  if (o.channel < 0) out.print(F("null"));
  else out.print(o.channel);
  out.print(F(",\"label\":\""));
  printEscaped(out, o.label);
  out.println(F("\",\"utc\":null,\"position\":null}"));
}

void EventPipeline::drainTo(Print& output, uint8_t maximum) {
  uint8_t processed = 0;
  while (count_ && processed < maximum) {
    printEvent(output, queue_[head_]);
    head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
    --count_;
    ++processed;
    ++emitted_;
  }
}
} // namespace callingcard
