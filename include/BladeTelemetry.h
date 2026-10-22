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
  void begin() {
    server_ = BLEDevice::createServer();
    server_->setCallbacks(&callbacks_);
    BLEService* service = server_->createService(serviceUuid());
    status_ = service->createCharacteristic(statusUuid(),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    status_->addDescriptor(new BLE2902());
    status_->setValue("v1,0,0,0,0,0");
    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(serviceUuid());
    advertising->setScanResponse(false);
    BLEDevice::startAdvertising();
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
    if (callbacks_.connected) status_->notify();
    if (!callbacks_.connected && hadConnection_) {
      server_->startAdvertising();
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
  bool hadConnection_ = false;
  uint32_t lastMs_ = 0;
};
}
#endif
