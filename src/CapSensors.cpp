#include "CapSensors.h"
#include "EventPipeline.h"

namespace callingcard {
volatile bool CapSensors::rxFlag_ = false;

#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif
void CapSensors::onPacket() {
  rxFlag_ = true;
}

void CapSensors::begin() {
  stage_ = "I2C";
  // Cap GNSS UART: module TX GPIO13 -> host RX GPIO15.
  gpsUart_.setRxBufferSize(2048);
  gpsUart_.begin(115200, SERIAL_8N1, 15, 13);
  gpsInitialized_ = true;

  // The board's working Meshtastic firmware demonstrated SX1262 SPI
  // initialization and RX/TX even though its I2C probe did not list 0x43.
  // Probe the expander independently: absence must not prevent a *receive-only*
  // SPI bring-up. RF antenna switch effectiveness remains unverified.
  stage_ = "I2C";
  i2cOk_ = m5::In_I2C.begin(I2C_NUM_0, 8, 9);
  if (i2cOk_) {
    stage_ = "I2C_PROBE";
    m5::In_I2C.scanID(detectedI2cAddresses_);
    for (uint8_t address = 0x08; address < 0x78; ++address) {
      if (!detectedI2cAddresses_[address]) continue;
      ++detectedI2cCount_;
      Serial.printf("CAP I2C found=0x%02X\n", address);
    }
    expanderAck_ = detectedI2cAddresses_[0x43];
    if (expanderAck_) {
      expanderOk_ = expander_.begin();
      if (expanderOk_) {
        expander_.setDirection(0, true);
        expander_.setHighImpedance(0, false);
        expander_.digitalWrite(0, true);
      }
    }
  }
  Serial.printf("CAP EXPANDER i2c=%d ack_0x43=%d driver=%d mode=%s\n",
                i2cOk_, expanderAck_, expanderOk_,
                expanderOk_ ? "P0_HIGH" : "UNVERIFIED");
  stage_ = "SX1262_INIT";

  // Explicit SPI pins shared with Cardputer SD. SD operations must be serialized.
  SPI.begin(40, 39, 14, 5);
  // Two explicitly selected receive-only PHY profiles; neither sends packets.
  // LongFast parameters are verified against Meshtastic's radio preset table
  // and on-device US channel frequency captured in reference firmware logs.
  // The sync word and preamble match Meshtastic's over-the-air PHY convention.
#ifndef CALLINGCARD_LORA_PROFILE
#define CALLINGCARD_LORA_PROFILE 1
#endif
#if CALLINGCARD_LORA_PROFILE == 1
  constexpr const char* profileName = "MESHTASTIC_US_LONGFAST";
  constexpr float frequencyMHz = 906.875;
  constexpr float bandwidthKHz = 250.0;
  constexpr uint8_t spreadingFactor = 11;
  constexpr uint8_t codingRate = 5;  // 4/5
  constexpr uint8_t syncWord = 0x2B;
  constexpr uint16_t preambleSymbols = 16;
#elif CALLINGCARD_LORA_PROFILE == 0
  constexpr const char* profileName = "LEGACY_915_TEST";
  constexpr float frequencyMHz = 915.0;
  constexpr float bandwidthKHz = 125.0;
  constexpr uint8_t spreadingFactor = 9;
  constexpr uint8_t codingRate = 5;
  constexpr uint8_t syncWord = 0x12;
  constexpr uint16_t preambleSymbols = 8;
#else
#error "Unsupported CALLINGCARD_LORA_PROFILE: use 0 (legacy) or 1 (Meshtastic US LongFast)"
#endif
  Serial.printf("CAP LORA PROFILE name=%s freq_mhz=%.3f bw_khz=%.1f sf=%u cr=4/%u sync=0x%02X preamble=%u rx_only=1\n",
                profileName, frequencyMHz, bandwidthKHz, spreadingFactor,
                codingRate, syncWord, preambleSymbols);
  radioState_ = radio_.begin(frequencyMHz, bandwidthKHz, spreadingFactor,
                             codingRate, syncWord, 10, preambleSymbols, 3.0, true);
  initCode_ = radioState_;
  Serial.printf("CAP SX1262 init status=%d\n", radioState_);
  if (radioReady()) {
    // RadioLib also enables SX126x DIO2 RF-switch control during begin;
    // request it explicitly and report the result for board comparison.
    const int16_t dio2State = radio_.setDio2AsRfSwitch(true);
    Serial.printf("CAP DIO2 RF switch status=%d\n", dio2State);
    if (dio2State != RADIOLIB_ERR_NONE) {
      radioState_ = dio2State;
      stage_ = "DIO2_SWITCH";
      return;
    }
    stage_ = "START_RX";
    radio_.setPacketReceivedAction(onPacket);
    const int state = radio_.startReceive();
    receiveCode_ = state;
    if (state != RADIOLIB_ERR_NONE) {
      radioState_ = state;
      Serial.printf("CAP startReceive failed=%d\n", state);
    } else {
      stage_ = "READY";
    }
  }
}

bool CapSensors::gnssUtc(char* buffer, size_t size) {
  Observation snapshot{};
  annotate(snapshot);
  if (!snapshot.hasUtc || size < sizeof(snapshot.utc)) return false;
  snprintf(buffer, size, "%s", snapshot.utc);
  return true;
}

void CapSensors::annotate(Observation& o) {
  // Snapshot ONLY fresh GNSS fields at observation time. Position and
  // time have independent validity; stale or uninitialized fields stay null.
  // Reject implausible dates rather than fabricating UTC from millis().
  if (gps_.date.isValid() && gps_.time.isValid() &&
      gps_.date.age() <= 2000 && gps_.time.age() <= 2000 &&
      gps_.date.year() >= 2020 && gps_.date.year() <= 2099 &&
      gps_.date.month() >= 1 && gps_.date.month() <= 12 &&
      gps_.date.day() >= 1 && gps_.date.day() <= 31 &&
      gps_.time.hour() <= 23 && gps_.time.minute() <= 59 &&
      gps_.time.second() <= 59) {
    snprintf(o.utc, sizeof(o.utc), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             gps_.date.year(), gps_.date.month(), gps_.date.day(),
             gps_.time.hour(), gps_.time.minute(), gps_.time.second());
    o.hasUtc = true;
  }
  if (gnssFix() && gps_.location.age() <= 2000) {
    o.latitude = gps_.location.lat();
    o.longitude = gps_.location.lng();
    o.hasPosition = true;
  }
}

bool CapSensors::gnssFix() const {
  return gpsInitialized_ && gps_.location.isValid() &&
         gps_.location.age() < 10000;
}

void CapSensors::poll(EventPipeline& events) {
  // Max bytes per pass to preserve main loop responsiveness.
  for (unsigned i = 0; i < 256 && gpsUart_.available(); ++i) {
    gps_.encode(gpsUart_.read());
    lastGpsSentenceMs_ = millis();
  }
  if (radioReady() && rxFlag_) {
    noInterrupts();
    rxFlag_ = false;
    interrupts();
    String payload;
    int rc = radio_.readData(payload);
    if (rc == RADIOLIB_ERR_NONE) {
      ++packets_;
      lastPacketRssi_ = static_cast<int16_t>(radio_.getRSSI());
      lastPacketSnrTenths_ = static_cast<int16_t>(radio_.getSNR() * 10.0f);
      // Queue metadata only; never place the received payload in telemetry.
      Observation observation{};
      observation.kind = Kind::LoRaPacket;
      observation.uptimeMs = millis();
      observation.rssiDbm = static_cast<int16_t>(radio_.getRSSI());
      observation.snrDb = radio_.getSNR();
      observation.channel = -1; // RF frequency is profile-defined, not Wi-Fi channel.
      annotate(observation);
      snprintf(observation.label, sizeof(observation.label), "sx1262");
      if (!events.enqueue(observation)) {
        Serial.println("CAP LORA event queue full; metadata dropped");
      }
      // Retain existing diagnostic output.
      Serial.printf("CAP LORA RX count=%lu rssi=%.1f snr=%.1f\n",
                    static_cast<unsigned long>(packets_),
                    radio_.getRSSI(), radio_.getSNR());
    } else {
      Serial.printf("CAP LORA read status=%d\n", rc);
    }
    const int restart = radio_.startReceive();
    if (restart != RADIOLIB_ERR_NONE) {
      radioState_ = restart;
      receiveCode_ = restart;
      stage_ = "RX_RESTART";
      Serial.printf("CAP LORA RX restart failure=%d\n", restart);
    }
  }
}

void CapSensors::logStatus() {
  Serial.printf("CAP DIAG stage=%s i2c=%d expander=%d sx1262_init=%d rx_start=%d radio_state=%d gps_chars=%lu gps_passed=%lu gps_failed=%lu gps_fix=%d\n",
    stage_, i2cOk_, expanderOk_, initCode_, receiveCode_, radioState_,
    static_cast<unsigned long>(gps_.charsProcessed()),
    static_cast<unsigned long>(gps_.passedChecksum()),
    static_cast<unsigned long>(gps_.failedChecksum()), gnssFix());
  Serial.printf("CAP I2C status initialized=%d target_0x43_ack=%d devices=%u expander_driver=%d\n",
                i2cOk_, expanderAck_, detectedI2cCount_, expanderOk_);
  Serial.print("CAP I2C addresses:");
  if (detectedI2cCount_ == 0) Serial.print(" none");
  for (uint8_t address = 0x08; address < 0x78; ++address) {
    if (detectedI2cAddresses_[address]) {
      Serial.printf(" 0x%02X", address);
    }
  }
  Serial.println();
  Serial.printf("CAP GPS bytes=%lu location=%s sats=%lu uart_age_ms=%lu lora_ready=%d packets=%lu\n",
    static_cast<unsigned long>(gps_.charsProcessed()),
    gnssFix() ? "FIX" : "NO_FIX",
    static_cast<unsigned long>(gps_.satellites.isValid() ? gps_.satellites.value() : 0),
    static_cast<unsigned long>(millis() - lastGpsSentenceMs_),
    radioReady(), static_cast<unsigned long>(packets_));
  if (gnssFix()) {
    Serial.printf("CAP GPS latitude=%.6f longitude=%.6f age_ms=%lu\n",
      gps_.location.lat(), gps_.location.lng(),
      static_cast<unsigned long>(gps_.location.age()));
  }
}
}
