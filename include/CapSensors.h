#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <RadioLib.h>
#include <TinyGPSPlus.h>
#include <M5Unified.h>
#include "utility/PI4IOE5V6408_Class.hpp"

namespace callingcard {
class EventPipeline;
struct Observation;
// Driver for the M5Stack Cap LoRa-1262 only. Receive-only by design.
// No GPS location is emitted without a current valid fix.
class CapSensors {
 public:
  void begin();
  void poll(EventPipeline& events);
  void logStatus();
  bool gnssFix() const;
  bool gnssUtc(char* buffer, size_t size);
  uint32_t satellites() { return gps_.satellites.isValid() ? gps_.satellites.value() : 0; }
  void annotate(Observation& observation);
  bool radioReady() const { return radioState_ == RADIOLIB_ERR_NONE; }
  uint32_t packetCount() const { return packets_; }
  int16_t lastPacketRssi() const { return lastPacketRssi_; }
  int16_t lastPacketSnrTenths() const { return lastPacketSnrTenths_; }
  const char* failureStage() const { return stage_; }
  int radioError() const { return radioState_; }
 private:
  HardwareSerial gpsUart_{1};
  TinyGPSPlus gps_{};
  SX1262 radio_{new Module(5, 4, 3, 6)};
  m5::PI4IOE5V6408_Class expander_{0x43, 400000, &m5::In_I2C};
  int radioState_ = -999;
  bool i2cOk_ = false;
  bool expanderOk_ = false;
  bool expanderAck_ = false;
  uint8_t detectedI2cCount_ = 0;
  // Full 7-bit address space; fixed allocation and no dynamic strings.
  bool detectedI2cAddresses_[128] = {};
  int initCode_ = -999;
  int receiveCode_ = -999;
  const char* stage_ = "NOT_STARTED";
  uint32_t packets_ = 0;
  int16_t lastPacketRssi_ = 0;
  int16_t lastPacketSnrTenths_ = 0;
  bool gpsInitialized_ = false;
  uint32_t lastGpsSentenceMs_ = 0;
  static volatile bool rxFlag_;
  static void onPacket();
};
}
