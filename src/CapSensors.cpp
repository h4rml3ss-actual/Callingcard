#include "CapSensors.h"

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

  // Cap-1262 requires IO-expander pin P0 high to enable RF antenna switch.
  // Fail closed when expander is unavailable; do not assume a different Cap.
  if (!m5::In_I2C.begin(I2C_NUM_0, 8, 9)) {
    Serial.println("CAP I2C: initialization failed; LoRa disabled");
    return;
  }
  i2cOk_ = true;
  stage_ = "I2C_PROBE";
  // Use M5Unified's existing I2C bus, not a second Wire instance which
  // could reconfigure the controller while other M5 peripherals are active.
  // A one-time bus scan avoids repeatedly probing active I2C peripherals.
  bool present[120] = {};
  m5::In_I2C.scanID(present);
  for (uint8_t address = 0x08; address < 0x78; ++address) {
    if (!present[address]) continue;
    ++detectedI2cCount_;
    Serial.printf("CAP I2C found=0x%02X\n", address);
  }
  expanderAck_ = present[0x43];
  Serial.printf("CAP I2C probe target=0x43 ack=%d devices=%u\n",
                expanderAck_, detectedI2cCount_);
  stage_ = "EXPANDER";
  if (!expanderAck_) {
    Serial.println("CAP I2C: 0x43 did not ACK; LoRa disabled");
    return;
  }
  if (!expander_.begin()) {
    Serial.println("CAP IO expander absent; LoRa disabled");
    return;
  }
  expanderOk_ = true;
  stage_ = "SX1262_INIT";
  expander_.setDirection(0, true);
  expander_.setHighImpedance(0, false);
  expander_.digitalWrite(0, true);

  // Explicit SPI pins shared with Cardputer SD. SD operations must be serialized.
  SPI.begin(40, 39, 14, 5);
  // US915 receive-only TEST profile, not a Meshtastic or arbitrary-LoRa decoder.
  // Match frequency, SF, BW, sync word and other settings to a known transmitter.
  radioState_ = radio_.begin(915.0, 125.0, 9, 5, 0x12, 10, 8, 3.0, true);
  initCode_ = radioState_;
  Serial.printf("CAP SX1262 init status=%d\n", radioState_);
  if (radioReady()) {
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

bool CapSensors::gnssFix() const {
  return gpsInitialized_ && gps_.location.isValid() &&
         gps_.location.age() < 10000;
}

void CapSensors::poll() {
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
      // Avoid collecting arbitrary payload content; report radio metadata only.
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
