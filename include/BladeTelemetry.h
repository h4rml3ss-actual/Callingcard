#pragma once
#include <Arduino.h>
#if CALLINGCARD_BLADE_POC
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
namespace callingcard {
// Experimental opt-in telemetry. No GPS coordinates, device names or payloads.
class BladeTelemetry {
 public:
  static constexpr const char* serviceUuid() { return "cabb1e00-ccad-4abc-a112-24fc00000001"; }
  static constexpr const char* statusUuid() { return "cabb1e00-ccad-4abc-a112-24fc00000002"; }
  static constexpr const char* alertUuid() { return "cabb1e00-ccad-4abc-a112-24fc00000003"; }
  // Small aggregate alert: E,<sequence mod 1000>,<W|B|L>. No identifiers.
  void queueAlert(char source) {
    if (source != 'W' && source != 'B' && source != 'L') return;
    // Coalesce repeated observations and rate-limit to one alert / 4 seconds.
    if (pendingSource_ == 0) pendingSource_ = source;
  }
  static constexpr const char* detailUuid() { return "cabb1e00-ccad-4abc-a112-24fc00000004"; }
#if CALLINGCARD_BLADE_DETAILS
  // Details are opt-in: public unencrypted GATT. Truncated labels only; no MACs or coordinates.
  void queueDetail(char kind, int channel, int rssi, const char* label) {
    if (kind != 'W' && kind != 'B' && kind != 'L') return;
    // Fixed-size bounded FIFO; preserve first 12 observations of each burst.
    if (detailCount_ >= 12) { ++detailDropped_; return; }
    snprintf(detailQueue_[detailTail_], sizeof(detailQueue_[0]), "D,%c,%d,%d,%.23s",
             kind, channel, rssi, label ? label : "");
    detailTail_ = (detailTail_ + 1) % 12;
    ++detailCount_;
  }
#endif
  bool connected() const { return callbacks_.connected; }
  void begin() {
    Serial.printf("BLADE init service=%s\n", serviceUuid());
    BLEDevice::setMTU(100); // Supports more than 8 label bytes when client negotiates MTU.
    server_ = BLEDevice::createServer();
    server_->setCallbacks(&callbacks_);
    BLEService* service = server_->createService(serviceUuid());
    status_ = service->createCharacteristic(statusUuid(),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    status_->addDescriptor(new BLE2902());
    alert_ = service->createCharacteristic(alertUuid(), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    alert_->addDescriptor(new BLE2902());
    alert_->setValue("E,0,-");
#if CALLINGCARD_BLADE_DETAILS
    detail_ = service->createCharacteristic(detailUuid(),
         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    detail_->addDescriptor(new BLE2902());
    detail_->setValue("D,-,0,0,-");
#endif
    status_->setValue("v1,0,0,0,0,0");
    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("BLADE advertising_started=1");
  }
  void tick(uint32_t wifi, uint32_t ble, uint32_t lora, bool gpsFix, bool recording) {
    if (millis()-lastMs_ < 1000) return;
    lastMs_ = millis();
    // Keep under 20 bytes (default ATT MTU payload).
    char value[20] = {};
    snprintf(value, sizeof(value), "1,%lu,%lu,%lu,%u,%u",
             (unsigned long)(wifi%1000),(unsigned long)(ble%1000),
             (unsigned long)(lora%1000), gpsFix?1:0, recording?1:0);
    status_->setValue(value);
#if CALLINGCARD_BLADE_DETAILS
    if (callbacks_.connected && detailCount_ && millis() - lastDetailMs_ >= 250) {
      detail_->setValue(detailQueue_[detailHead_]);
      detail_->notify();
      detailHead_ = (detailHead_ + 1) % 12;
      --detailCount_;
      lastDetailMs_ = millis();
    }
#endif
    if (callbacks_.connected && pendingSource_ && millis() - lastAlertMs_ >= 4000) {
      char alertValue[18] = {};
      snprintf(alertValue, sizeof(alertValue), "E,%lu,%c",
               (unsigned long)(++alertSequence_ % 1000), pendingSource_);
      pendingSource_ = 0;
      lastAlertMs_ = millis();
      alert_->setValue(alertValue);
      alert_->notify();
      Serial.printf("BLADE alert=%s\n", alertValue);
    }
    if (callbacks_.connected) {
      ++notificationAttempts_;
      status_->notify();
      if (millis() - lastLogMs_ >= 10000) {
        Serial.printf("BLADE connected=1 notify=%s attempts=%lu\n", value, (unsigned long)notificationAttempts_);
        lastLogMs_ = millis();
      }
    }
    if (!callbacks_.connected && hadConnection_) {
      server_->startAdvertising();
      Serial.println("BLADE advertising_restarted=1");
    }
    if (callbacks_.connected != hadConnection_) {
      Serial.printf("BLADE connection_changed=%u\n", callbacks_.connected ? 1 : 0);
    }
    hadConnection_ = callbacks_.connected;
  }
 private:
  class Callbacks : public BLEServerCallbacks {
   public:
    volatile bool connected = false;
    void onConnect(BLEServer*) override { connected=true; }
    void onDisconnect(BLEServer*) override { connected=false; }
  } callbacks_;
  BLEServer* server_ = nullptr;
  BLECharacteristic* status_ = nullptr;
  BLECharacteristic* alert_ = nullptr;
#if CALLINGCARD_BLADE_DETAILS
  BLECharacteristic* detail_ = nullptr;
  char detailQueue_[12][48] = {};
  uint8_t detailHead_ = 0, detailTail_ = 0, detailCount_ = 0;
  uint32_t detailDropped_ = 0;
  uint32_t lastDetailMs_ = 0;
#endif
  char pendingSource_ = 0;
  uint32_t lastAlertMs_ = 0;
  uint32_t alertSequence_ = 0;
  bool hadConnection_ = false;
  uint32_t lastMs_ = 0;
  uint32_t lastLogMs_ = 0;
  uint32_t notificationAttempts_ = 0;
};
}
#endif
