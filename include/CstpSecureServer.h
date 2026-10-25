#pragma once
// CSTP v1 Cardputer prototype; deliberately separate from legacy BladeTelemetry.
// Security scope: untrusted BLE transport, NOT flash extraction/debugger attackers.
// Wire format: docs/CSTP_V1_SECURITY_PROTOCOL.md
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/gcm.h>
#include <string.h>

namespace callingcard {
class CstpSecureServer {
 public:
  static constexpr const char* kService = "cabb1e00-ccad-4abc-a112-24fc00000100";
  static constexpr const char* kControl = "cabb1e00-ccad-4abc-a112-24fc00000101";
  static constexpr const char* kData = "cabb1e00-ccad-4abc-a112-24fc00000102";
  void begin() {
    Preferences prefs;
    if (prefs.begin("cstp", true)) {
      provisioned_ = prefs.getBytesLength("psk") == sizeof(psk_) &&
                     prefs.getBytes("psk", psk_, sizeof(psk_)) == sizeof(psk_);
      prefs.end();
    }
    Serial.printf("CSTP service=isolated provisioned=%u (no plaintext telemetry)\n", provisioned_ ? 1 : 0);
    server_ = BLEDevice::createServer();
    server_->setCallbacks(&callbacks_);
    callbacks_.owner = this;
    BLEService* svc = server_->createService(kService);
    control_ = svc->createCharacteristic(kControl, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ |
                                                 BLECharacteristic::PROPERTY_NOTIFY);
    data_ = svc->createCharacteristic(kData, BLECharacteristic::PROPERTY_NOTIFY);
    control_->setCallbacks(&controlCallbacks_);
    controlCallbacks_.owner = this;
    control_->addDescriptor(new BLE2902());
    data_->addDescriptor(new BLE2902());
    svc->start();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(kService);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
  }
  // Non-sensitive aggregate payloads are also gated: no secure session, no transfer.
  void tick(uint32_t wifi, uint32_t ble, uint32_t lora, bool fix, bool recording) {
    serviceProvisioning();
    if (!connected_ || !authorized_ || millis() - lastSendMs_ < 1000) return;
    lastSendMs_ = millis();
    char msg[24];
    const int size = snprintf(msg, sizeof(msg), "1,%lu,%lu,%lu,%u,%u",
       (unsigned long)(wifi % 1000), (unsigned long)(ble % 1000),
       (unsigned long)(lora % 1000), fix ? 1 : 0, recording ? 1 : 0);
    if (size < 0 || size >= (int)sizeof(msg) || size > 40) return;
    // The logical GCM frame requires ATT MTU >= 3+21+size+16.
    if (BLEDevice::getMTU() < (uint16_t)(3 + 21 + size + 16)) return;
    uint8_t frame[21 + 40 + 16] = {};
    frame[0] = 1; frame[1] = 1; frame[2] = 0x10;
    memcpy(frame + 3, sid_, 8);
    if (sendSeq_ == UINT64_MAX) { resetSession(); return; }
    put64(frame + 11, sendSeq_);
    frame[19] = 0; frame[20] = (uint8_t)size;
    uint8_t nonce[12] = {};
    memcpy(nonce, prefix_, 4); put64(nonce + 4, sendSeq_);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    const int configured = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key_, 256);
    int ok = -1;
    if (configured == 0) {
      ok = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, size,
                  nonce, 12, frame, 21, (const unsigned char*)msg,
                  frame + 21, 16, frame + 21 + size);
    }
    mbedtls_gcm_free(&gcm);
    if (ok != 0) { resetSession(); return; }
    data_->setValue(frame, 21 + size + 16);
    data_->notify();
    ++sendSeq_;
  }
  bool authorized() const { return authorized_; }
  bool provisioned() const { return provisioned_; }
 private:
  static constexpr uint32_t kHandshakeTimeoutMs = 30000;
  class Callbacks : public BLEServerCallbacks {
   public:
    CstpSecureServer* owner = nullptr;
    void onConnect(BLEServer*) override {
      owner->connected_ = true;
      owner->resetSession();
      owner->startChallenge();
    }
    void onDisconnect(BLEServer*) override {
      owner->connected_ = false;
      owner->resetSession();
      BLEDevice::startAdvertising();
    }
  } callbacks_;
  class ControlCallbacks : public BLECharacteristicCallbacks {
   public:
    CstpSecureServer* owner = nullptr;
    void onWrite(BLECharacteristic* c) override {
      std::string v = c->getValue();
      owner->onControl(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
  } controlCallbacks_;
  BLEServer* server_ = nullptr;
  BLECharacteristic* control_ = nullptr;
  BLECharacteristic* data_ = nullptr;
  bool connected_ = false, authorized_ = false, provisioned_ = false, awaitingProof_ = false;
  uint8_t psk_[32] = {}, card_[32] = {}, blade_[32] = {};
  uint8_t sid_[8] = {}, key_[32] = {}, prefix_[4] = {};
  uint64_t sendSeq_ = 0;
  uint32_t challengeStarted_ = 0, lastSendMs_ = 0;
  String provisionBuffer_;
  uint32_t provisionUntil_ = 0;
  static void put64(uint8_t* d, uint64_t v) {
    for (int i = 7; i >= 0; --i) { d[i] = (uint8_t)v; v >>= 8; }
  }
  static bool digest(const uint8_t* data, size_t len, uint8_t out[32]) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && mbedtls_md(md, data, len, out) == 0;
  }
  static bool hmac(const uint8_t* key, size_t kl, const uint8_t* data, size_t len, uint8_t out[32]) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && mbedtls_md_hmac(md, key, kl, data, len, out) == 0;
  }
  static bool equal32(const uint8_t* x, const uint8_t* y) {
    uint8_t diff = 0;
    for (size_t i=0; i<32; ++i) diff |= x[i]^y[i];
    return diff == 0;
  }
  void resetSession() {
    authorized_ = false; awaitingProof_ = false; sendSeq_ = 0;
    memset(key_,0,sizeof(key_)); memset(prefix_,0,sizeof(prefix_));
    memset(card_,0,sizeof(card_)); memset(blade_,0,sizeof(blade_)); memset(sid_,0,sizeof(sid_));
  }
  void startChallenge() {
    if (!provisioned_) return;
    esp_fill_random(card_,32);
    uint8_t hello[34] = {1,1};
    memcpy(hello+2,card_,32);
    control_->setValue(hello,sizeof(hello));
    // Client reads challenge from control notification only after subscribing.
    // Delayed HELLO notification is handled by the app client in PR55.
    challengeStarted_ = millis();
    awaitingProof_ = true;
    Serial.println("CSTP handshake=hello_ready");
  }
  bool hkdfExpand(const uint8_t prk[32], const char* label, const uint8_t sid[8],
                  uint8_t* output, size_t n) {
    const size_t labelLen = strlen(label);
    if (labelLen + 10 > 96 || n > 32) return false;
    uint8_t input[96] = {};
    memcpy(input,label,labelLen); input[labelLen] = 0;
    memcpy(input+labelLen+1,sid,8);
    input[labelLen+9] = 1;
    uint8_t result[32];
    const bool ok = hmac(prk,32,input,labelLen+10,result);
    if (ok) memcpy(output,result,n);
    memset(result,0,sizeof(result)); return ok;
  }
  void onControl(const uint8_t* msg, size_t size) {
    if (!connected_ || !provisioned_ || !awaitingProof_ ||
        millis() - challengeStarted_ > kHandshakeTimeoutMs || size != 66 ||
        msg[0] != 2 || msg[1] != 1) { resetSession(); return; }
    memcpy(blade_,msg+2,32);
    uint8_t transcript[72] = {};
    memcpy(transcript,"CSTP-v1",7); transcript[7]=0;
    memcpy(transcript+8,card_,32); memcpy(transcript+40,blade_,32);
    uint8_t expected[32], out[32];
    const char* bproof = "CSTP-v1 blade-proof";
    uint8_t tagged[128] = {};
    const size_t taglen = strlen(bproof);
    memcpy(tagged,bproof,taglen); tagged[taglen]=0;
    memcpy(tagged+taglen+1,transcript,sizeof(transcript));
    bool ok = hmac(psk_,32,tagged,taglen+1+sizeof(transcript),expected);
    if (!ok || !equal32(expected,msg+34)) { resetSession(); return; }
    const char* sp = "CSTP-v1 cardputer-proof";
    const size_t plen = strlen(sp);
    memcpy(tagged,sp,plen); tagged[plen]=0;
    memcpy(tagged+plen+1,transcript,sizeof(transcript));
    if (!hmac(psk_,32,tagged,plen+1+sizeof(transcript),out)) { resetSession(); return; }
    uint8_t signedProof[34] = {3,1}; memcpy(signedProof+2,out,32);
    uint8_t salted[128] = {};
    memcpy(salted,"CSTP-v1 salt",12); salted[12]=0;
    memcpy(salted+13,transcript,sizeof(transcript));
    uint8_t salt[32], prk[32];
    if (!digest(salted,13+sizeof(transcript),salt) || !hmac(salt,32,psk_,32,prk)) { resetSession(); return; }
    uint8_t sessionInput[96] = {};
    memcpy(sessionInput,"CSTP-v1 sid",11); sessionInput[11]=0;
    memcpy(sessionInput+12,transcript,sizeof(transcript));
    uint8_t sidDigest[32];
    if (!digest(sessionInput,12+sizeof(transcript),sidDigest)) { resetSession(); return; }
    memcpy(sid_,sidDigest,8);
    if (!hkdfExpand(prk,"CSTP-v1 key c2b",sid_,key_,32) ||
        !hkdfExpand(prk,"CSTP-v1 nonce c2b",sid_,prefix_,4)) { resetSession(); return; }
    control_->setValue(signedProof,sizeof(signedProof));
    control_->notify();
    awaitingProof_ = false;
    authorized_ = true;
    challengeStarted_ = 0;
    Serial.println("CSTP handshake=server_proof_sent (client confirmation required)");
  }
  // Developer-only physical USB provisioning: 30-second explicit boot window.
  // Disable in production; unencrypted NVS is vulnerable to physical extraction.
  void serviceProvisioning() {
#if CALLINGCARD_CSTP_DEV_USB_PROVISION
    if (provisioned_ || millis() > 30000) return;
    while (Serial.available()) {
      const char ch = (char)Serial.read();
      if (ch == '\r') continue;
      if (ch != '\n' && provisionBuffer_.length() < 80) { provisionBuffer_ += ch; continue; }
      if (provisionBuffer_.startsWith("CSTP-SET ") && provisionBuffer_.length() == 73) {
        uint8_t candidate[32] = {}; bool valid = true;
        for (int i=0;i<32;i++) {
          const int a=hexDigit(provisionBuffer_[9+i*2]), b=hexDigit(provisionBuffer_[10+i*2]);
          if (a<0 || b<0) { valid=false; break; }
          candidate[i]=(a<<4)|b;
        }
        uint8_t any = 0;
        for (uint8_t x : candidate) any |= x;
        if (valid && any != 0) {
          Preferences prefs;
          if (prefs.begin("cstp",false)) {
            provisioned_ = prefs.putBytes("psk",candidate,32)==32;
            prefs.end();
            if (provisioned_) memcpy(psk_,candidate,32);
          }
        }
        memset(candidate,0,32);
        Serial.printf("CSTP provision_result=%s\n",provisioned_?"ok":"failed");
      }
      provisionBuffer_ = "";
    }
#endif
  }
  static int hexDigit(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
  }
};
} // namespace callingcard
