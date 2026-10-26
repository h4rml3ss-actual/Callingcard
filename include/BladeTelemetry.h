#pragma once
#include <Arduino.h>
#include "DetectionEngine.h"
#if CALLINGCARD_BLADE_GEO_V2
#include "GeoFrames.h"
#if CALLINGCARD_BLADE_EVIDENCE_V3
#include "EvidenceFrames.h"
#endif
#endif
#if CALLINGCARD_BLADE_POC
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#if CALLINGCARD_BLADE_SECURE
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
#endif
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
  // C,<type>,<confidence>,<RSSI>,<source>; bounded to legacy ATT payload.
  void queueDetection(DetectionType type, Confidence confidence, int rssi, char source) {
    if (type==DetectionType::Unknown || confidence==Confidence::None ||
        (source!='W' && source!='B') || rssi < -127 || rssi > 20) return;
    // Single pending slot: new candidates never overwrite an unsent alert.
    if (pendingDetection_[0]) return;
    const char rank=confidence==Confidence::High?'H':
                    confidence==Confidence::Medium?'M':'L';
    snprintf(pendingDetection_,sizeof(pendingDetection_),"C,%s,%c,%d,%c",
             detectionTypeName(type),rank,rssi,source);
  }
#if CALLINGCARD_BLADE_GEO_V2
  // Entire event comes from one original observation snapshot. No mutation
  // until the queue has been transmitted, and no late GPS lookup.
  bool queueGeoDetection(const Observation& observation) {
    if (geoActive_) return false;
    GeoFrames next{};
    const uint16_t seq=static_cast<uint16_t>((geoSequence_+1)%1000);
    if(!formatGeoFrames(observation,seq,next))return false;
#if CALLINGCARD_BLADE_EVIDENCE_V3
    EvidenceFrames evidence{};
    if(!formatEvidenceFrames(observation,seq,evidence))return false;
    evidenceFrames_=evidence;
#endif
    geoSequence_=seq;
    geoFrames_=next;
    geoActive_=true;
    geoPhase_=0;
    return true;
  }
#endif
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
#if CALLINGCARD_BLADE_SECURE
  bool authorized() const { return securityCallbacks_.authenticated; }
#endif
  void begin() {
    Serial.printf("BLADE init service=%s\n", serviceUuid());
    BLEDevice::setMTU(100); // Supports more than 8 label bytes when client negotiates MTU.
#if CALLINGCARD_BLADE_SECURE
    // Provision a fresh passkey at boot, never store a default credential in Git.
    // First enrollment is interactive; bonded reconnects reuse OS/ESP bond keys.
    pin_ = 100000 + (esp_random() % 900000);
    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    security->setCapability(ESP_IO_CAP_OUT);
    // Arduino-ESP32 2.0.x: configure Bluedroid GAP directly.
    securityCallbacks_.passkey = pin_;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &pin_, sizeof(pin_));
    BLEDevice::setSecurityCallbacks(&securityCallbacks_);
    Serial.printf("BLADE PAIRING CODE %06lu (first-time enrollment only)\n",
                  (unsigned long)pin_);
#endif
    server_ = BLEDevice::createServer();
    server_->setCallbacks(&callbacks_);
    BLEService* service = server_->createService(serviceUuid());
    status_ = service->createCharacteristic(statusUuid(),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if CALLINGCARD_BLADE_SECURE
    status_->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
#endif
    BLE2902* statusCccd = new BLE2902();
#if CALLINGCARD_BLADE_SECURE
    statusCccd->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
#endif
    status_->addDescriptor(statusCccd);
    alert_ = service->createCharacteristic(alertUuid(), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if CALLINGCARD_BLADE_SECURE
    alert_->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
#endif
    BLE2902* alertCccd = new BLE2902();
#if CALLINGCARD_BLADE_SECURE
    alertCccd->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
#endif
    alert_->addDescriptor(alertCccd);
    alert_->setValue("E,0,-");
#if CALLINGCARD_BLADE_DETAILS
    detail_ = service->createCharacteristic(detailUuid(),
         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if CALLINGCARD_BLADE_SECURE
    detail_->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
#endif
    BLE2902* detailCccd = new BLE2902();
#if CALLINGCARD_BLADE_SECURE
    detailCccd->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
#endif
    detail_->addDescriptor(detailCccd);
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
#if CALLINGCARD_BLADE_SECURE
    const bool secureReady = securityCallbacks_.authenticated;
    // Temporary development reminder: USB serial only, not GATT.
    if (!secureReady && millis() - lastPinReminderMs_ >= 15000) {
      lastPinReminderMs_ = millis();
      const uint32_t currentCode = securityCallbacks_.notifiedPasskey
        ? securityCallbacks_.displayedPasskey : pin_;
      Serial.printf("BLADE PAIRING CODE %06lu | %s\n",
                    (unsigned long)currentCode,
                    securityCallbacks_.notifiedPasskey
                      ? "active pairing code (from BLE stack)" : "configured code; awaiting pairing");
    }
#endif
    lastMs_ = millis();
    // Keep under 20 bytes (default ATT MTU payload).
    char value[20] = {};
    snprintf(value, sizeof(value), "1,%lu,%lu,%lu,%u,%u",
             (unsigned long)(wifi%1000),(unsigned long)(ble%1000),
             (unsigned long)(lora%1000), gpsFix?1:0, recording?1:0);
    status_->setValue(value);
#if CALLINGCARD_BLADE_DETAILS
    if (callbacks_.connected &&
#if CALLINGCARD_BLADE_SECURE
        secureReady &&
#endif
        detailCount_ && millis() - lastDetailMs_ >= 250) {
      detail_->setValue(detailQueue_[detailHead_]);
      detail_->notify();
      detailHead_ = (detailHead_ + 1) % 12;
      --detailCount_;
      lastDetailMs_ = millis();
    }
#endif
#if CALLINGCARD_BLADE_GEO_V2
    if (callbacks_.connected &&
#if CALLINGCARD_BLADE_SECURE
        secureReady &&
#endif
        geoActive_) {
      const char* frame=nullptr;
      if(geoPhase_==0)frame=geoFrames_.header;
      else if(geoPhase_==1 && geoFrames_.hasPosition)frame=geoFrames_.position;
      else if(geoPhase_==2 && geoFrames_.hasTimestamp)frame=geoFrames_.timestamp;
#if CALLINGCARD_BLADE_EVIDENCE_V3
      else if(geoPhase_>=3 && geoPhase_<static_cast<uint8_t>(3+evidenceFrames_.count))
        frame=evidenceFrames_.fragments[geoPhase_-3];
#endif
      if(frame) {
        alert_->setValue(frame);
        alert_->notify();
        Serial.printf("BLADE geo=%s\\n",frame);
      }
      ++geoPhase_;
      uint8_t phases=3;
#if CALLINGCARD_BLADE_EVIDENCE_V3
      phases=static_cast<uint8_t>(3+evidenceFrames_.count);
#endif
      if(geoPhase_>=phases){geoActive_=false;geoPhase_=0;}
    }
#endif
    if (callbacks_.connected &&
#if CALLINGCARD_BLADE_SECURE
        secureReady &&
#endif
        pendingDetection_[0]) {
      alert_->setValue(pendingDetection_);
      alert_->notify();
      Serial.printf("BLADE detection=%s\\n",pendingDetection_);
      pendingDetection_[0]=0;
      lastAlertMs_=millis();
    }
    if (callbacks_.connected &&
#if CALLINGCARD_BLADE_SECURE
        secureReady &&
#endif
        pendingSource_ && millis() - lastAlertMs_ >= 4000) {
      char alertValue[18] = {};
      snprintf(alertValue, sizeof(alertValue), "E,%lu,%c",
               (unsigned long)(++alertSequence_ % 1000), pendingSource_);
      pendingSource_ = 0;
      lastAlertMs_ = millis();
      alert_->setValue(alertValue);
      alert_->notify();
      Serial.printf("BLADE alert=%s\n", alertValue);
    }
    if (callbacks_.connected
#if CALLINGCARD_BLADE_SECURE
        && secureReady
#endif
    ) {
      ++notificationAttempts_;
      status_->notify();
      if (millis() - lastLogMs_ >= 10000) {
        Serial.printf("BLADE connected=1 notify=%s attempts=%lu\n", value, (unsigned long)notificationAttempts_);
        lastLogMs_ = millis();
      }
    }
    if (!callbacks_.connected && hadConnection_) {
#if CALLINGCARD_BLADE_SECURE
      securityCallbacks_.authenticated = false;
#endif
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
#if CALLINGCARD_BLADE_SECURE
  class SecurityCallbacks : public BLESecurityCallbacks {
   public:
    volatile bool authenticated = false;
    uint32_t passkey = 0;
    uint32_t onPassKeyRequest() override {
      Serial.println("BLADE AUTH event=passkey_request");
      return passkey;
    }
    volatile uint32_t displayedPasskey = 0;
    volatile bool notifiedPasskey = false;
    void onPassKeyNotify(uint32_t passkey) override {
      // Show the actual passkey supplied by Bluedroid for this pairing attempt.
      displayedPasskey = passkey;
      notifiedPasskey = true;
      Serial.printf("BLADE AUTH event=passkey_notify pairing_code=%06lu (enter on Blade)\n",
                    (unsigned long)passkey);
    }
    bool onSecurityRequest() override {
      Serial.println("BLADE AUTH event=security_request");
      return true;
    }
    // Display-only pairing must never silently approve numeric comparison.
    bool onConfirmPIN(uint32_t) override {
      Serial.println("BLADE AUTH event=numeric_comparison rejected_by_policy");
      return false;
    }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
      authenticated = result.success && ((result.auth_mode & ESP_LE_AUTH_REQ_MITM) != 0);
      notifiedPasskey = false;
      Serial.printf("BLADE secure_auth=%u success=%u mode=%u fail_reason=%u\n",
         authenticated?1:0, result.success?1:0,
         (unsigned)result.auth_mode, (unsigned)result.fail_reason);
    }
  } securityCallbacks_;
  uint32_t pin_ = 0;
  uint32_t lastPinReminderMs_ = 0;
#endif
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
#if CALLINGCARD_BLADE_GEO_V2
  GeoFrames geoFrames_{};
#if CALLINGCARD_BLADE_EVIDENCE_V3
  EvidenceFrames evidenceFrames_{};
#endif
  bool geoActive_=false;
  uint8_t geoPhase_=0;
  uint16_t geoSequence_=0;
#endif
  char pendingDetection_[32] = {};
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
