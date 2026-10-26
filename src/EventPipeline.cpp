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
  if (activity_) activity_->add(observation);
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
  if (o.kind == Kind::WifiNetwork && o.wifiBssid[0]) {
    out.print(F("\",\"bssid\":\""));
    out.print(o.wifiBssid);
  } else if (o.kind == Kind::BleAdvertisement && o.bleAddress[0]) {
    out.print(F("\",\"ble_address\":\""));
    out.print(o.bleAddress);
  }
  if (o.kind == Kind::LoRaPacket) {
    out.print(F("\",\"snr_db\":"));
    out.print(o.snrDb, 1);
  }
  out.print(F("\",\"utc\":"));
  if (o.hasUtc) {
    out.print('"');
    out.print(o.utc);
    out.print('"');
  } else out.print(F("null"));
  if (o.hasDetection) {
    out.print(F(",\"detection\":{\"type\":\""));
    out.print(o.detectionType);
    out.print(F("\",\"confidence\":\""));
    out.print(o.detectionConfidence);
    out.print(F("\",\"rule\":\""));
    out.print(o.detectionRule);
    out.print(F("\"}"));
  }
  out.print(F(",\"position\":"));
  if (o.hasPosition) {
    out.print(F("{\"lat\":"));
    out.print(o.latitude, 6);
    out.print(F(",\"lon\":"));
    out.print(o.longitude, 6);
    out.print('}');
  } else out.print(F("null"));
  out.println('}');
}

// Write each event once into bounded RAM before fanning out to serial and SD.
// If an unusually long event exceeds the buffer, preserve serial visibility
// and signal the mirror failure instead of writing a truncated JSONL record.
namespace {
class FixedLine : public Print {
 public:
  size_t write(uint8_t b) override {
    if (length >= sizeof(bytes)) { overflow = true; return 0; }
    bytes[length++] = b;
    return 1;
  }
  size_t write(const uint8_t* data, size_t size) override {
    size_t n = 0;
    for (; n < size; ++n) if (write(data[n]) != 1) break;
    return n;
  }
  uint8_t bytes[512]{};
  size_t length = 0;
  bool overflow = false;
};
}
void EventPipeline::drainTo(Print& output, uint8_t maximum, Print* mirror, SessionStats* stats) {
  uint8_t processed = 0;
  while (count_ && processed < maximum) {
    FixedLine line;
    printEvent(line, queue_[head_]);
    if (!line.overflow) {
      output.write(line.bytes, line.length);
      bool saved = false;
      if (mirror) {
        saved = mirror->write(line.bytes, line.length) == line.length;
        if (!saved) ++mirrorErrors_;
      }
      if (stats) stats->onEvent(queue_[head_], saved);
    } else {
      ++mirrorErrors_;
      if (stats) stats->onEvent(queue_[head_], false);
      output.println(F("PIPE event serialization overflow"));
    }
    head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
    --count_;
    ++processed;
    ++emitted_;
  }
}
} // namespace callingcard
