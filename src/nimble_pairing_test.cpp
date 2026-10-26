// PR56: isolated NimBLE passkey/MITM/bonding diagnostic, NOT the scanner.
// Build ONLY with: pio run -e cardputer-nimble-pairing-test
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_system.h>

namespace {
constexpr const char* kService = "cabb1e00-ccad-4abc-a112-24fc00000001";
constexpr const char* kStatus = "cabb1e00-ccad-4abc-a112-24fc00000002";
uint32_t passkey = 0;
volatile bool connected = false;
volatile bool authenticated = false;

class SecurityEvents : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
    connected = true;
    authenticated = false;
    Serial.println("NIMBLE connected=1, waiting_for_authenticated_bond=1");
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    connected = false;
    authenticated = false;
    Serial.printf("NIMBLE disconnected reason=%d\n", reason);
    NimBLEDevice::startAdvertising();
  }
  uint32_t onPassKeyDisplay() override {
    Serial.printf("NIMBLE ENTER_ON_BLADE passkey=%06lu\n", (unsigned long)passkey);
    return passkey;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    authenticated = info.isEncrypted() && info.isAuthenticated() && info.isBonded();
    Serial.printf("NIMBLE auth encrypted=%u mitm=%u bonded=%u accepted=%u\n",
                  info.isEncrypted(), info.isAuthenticated(), info.isBonded(), authenticated ? 1 : 0);
    if (!authenticated) {
      Serial.println("NIMBLE authentication_rejected=1 (no unsecured fallback)");
    }
  }
};
SecurityEvents events;
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("NIMBLE pairing isolation: NO SD / LoRa / WiFi / scans / CSTP");
  passkey = 100000 + (esp_random() % 900000);
  NimBLEDevice::init("Callingcard");
  NimBLEDevice::setSecurityAuth(true, true, true); // bond, MITM, Secure Connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(passkey);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&events);
  server->advertiseOnDisconnect(true);
  NimBLEService* service = server->createService(kService);
  // Important: no publicly readable status; authenticated read access only.
  NimBLECharacteristic* status = service->createCharacteristic(
    kStatus, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC |
             NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::NOTIFY);
  status->setValue("1,0,0,0,0,0");
  service->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kService);
  advertising->setName("Callingcard");
  advertising->start();
  Serial.printf("NIMBLE advertising=1 pin=%06lu secure_only=1\n", (unsigned long)passkey);
}
void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 10000) {
    last = millis();
    Serial.printf("NIMBLE state connected=%u authenticated=%u\n",
                  connected ? 1 : 0, authenticated ? 1 : 0);
  }
  delay(50);
}
