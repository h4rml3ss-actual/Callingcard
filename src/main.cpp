// Callingcard PR3: scheduled Wi-Fi survey and BLE advertisement discovery.
// This is a field-observation smoke test, not continuous packet capture.
#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include "Observation.h"
#include "CapSensors.h"
#include "EventPipeline.h"

namespace {
constexpr uint32_t kPauseMs = 2500;
constexpr uint32_t kBleScanSeconds = 4;
constexpr int kMaxLines = 5;
uint32_t scanNumber = 0;
uint32_t nextScanMs = 0;
uint32_t wifiCount = 0;
uint32_t bleCount = 0;
uint32_t bleCallbacks = 0;
uint32_t errorCount = 0;
uint32_t wifiElapsedMs = 0;
uint32_t bleElapsedMs = 0;
uint32_t lastScanEndMs = 0;
uint32_t nextCapLogMs = 0;
callingcard::CapSensors cap;
callingcard::EventPipeline pipeline;
uint32_t nextPipelineLogMs = 0;
bool scanWifiNext = true;
BLEScan* bleScanner = nullptr;
callingcard::Observation latest[kMaxLines]{};
int latestCount = 0;

void addObservation(const callingcard::Observation& observation) {
  if (latestCount < kMaxLines) {
    latest[latestCount++] = observation;
  }
  pipeline.enqueue(observation);
}

class AdvertisedCallback : public BLEAdvertisedDeviceCallbacks {
 void onResult(BLEAdvertisedDevice device) override {
   ++bleCallbacks;
   if (latestCount >= kMaxLines) return;
   callingcard::Observation item{};
   item.kind = callingcard::Kind::BleAdvertisement;
   item.uptimeMs = millis();
   item.rssiDbm = device.getRSSI();
   item.channel = -1;
   // Do not retain advertised device addresses by default.
   const std::string name = device.getName();
   snprintf(item.label, sizeof(item.label), "%s",
            name.empty() ? "<unnamed BLE>" : name.c_str());
   addObservation(item);
 }
};
AdvertisedCallback callback;

void drawStatus(const char* mode) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextFont(1);
  d.setTextSize(1);
  d.setTextColor(TFT_GREEN, TFT_BLACK);
  d.setCursor(3, 2);
  d.println("CALLINGCARD  ADV");
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.printf("%s #%lu\n", mode, static_cast<unsigned long>(scanNumber));
  d.printf("WIFI:%lu BLE:%lu\n", static_cast<unsigned long>(wifiCount),
           static_cast<unsigned long>(bleCount));
  d.printf("W:%lums B:%lums\n", static_cast<unsigned long>(wifiElapsedMs),
           static_cast<unsigned long>(bleElapsedMs));
  d.printf("Heap:%u Err:%lu\n", ESP.getFreeHeap(),
           static_cast<unsigned long>(errorCount));
  for (int i = 0; i < latestCount; ++i) {
    d.printf("%c %-15.15s %4d\n",
            latest[i].kind == callingcard::Kind::WifiNetwork ? 'W' : 'B',
            latest[i].label, latest[i].rssiDbm);
  }
  d.printf("Q:%u Drop:%lu\n", pipeline.queued(), static_cast<unsigned long>(pipeline.dropped()));
  d.printf("CAP:%s E:%d\n", cap.failureStage(), cap.radioError());
  d.printf("GPS:%s LORA:%s RX:%lu\n", cap.gnssFix() ? "FIX" : "NOFIX", cap.radioReady() ? "OK" : "ERR", static_cast<unsigned long>(cap.packetCount()));
}

void surveyWifi() {
  latestCount = 0;
  const uint32_t beginMs = millis();
  // Synchronous survey intentionally has monitoring blind spots.
  const int found = WiFi.scanNetworks(false, true);
  wifiElapsedMs = millis() - beginMs;
  if (found < 0) {
    ++errorCount;
    wifiCount = 0;
  } else {
    wifiCount = static_cast<uint32_t>(found);
    for (int i = 0; i < found && latestCount < kMaxLines; ++i) {
      callingcard::Observation item{};
      item.kind = callingcard::Kind::WifiNetwork;
      item.uptimeMs = millis();
      item.rssiDbm = WiFi.RSSI(i);
      item.channel = WiFi.channel(i);
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) ssid = "<hidden>";
      snprintf(item.label, sizeof(item.label), "%s", ssid.c_str());
      addObservation(item);
    }
  }
  WiFi.scanDelete();
  Serial.printf("CYCLE mode=wifi count=%lu duration_ms=%lu heap=%u errors=%lu\n",
       static_cast<unsigned long>(wifiCount),
       static_cast<unsigned long>(wifiElapsedMs), ESP.getFreeHeap(),
       static_cast<unsigned long>(errorCount));
}

void surveyBle() {
  latestCount = 0;
  bleCallbacks = 0;
  const uint32_t beginMs = millis();
  // Scanning uses finite, bounded windows; no BLE peripheral connection yet.
  BLEScanResults result = bleScanner->start(kBleScanSeconds, false);
  bleElapsedMs = millis() - beginMs;
  bleCount = static_cast<uint32_t>(result.getCount());
  bleScanner->clearResults();
  Serial.printf("CYCLE mode=ble unique=%lu callbacks=%lu duration_ms=%lu heap=%u errors=%lu\n",
       static_cast<unsigned long>(bleCount),
       static_cast<unsigned long>(bleCallbacks),
       static_cast<unsigned long>(bleElapsedMs), ESP.getFreeHeap(),
       static_cast<unsigned long>(errorCount));
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  BLEDevice::init("Callingcard");
  bleScanner = BLEDevice::getScan();
  bleScanner->setAdvertisedDeviceCallbacks(&callback);
  bleScanner->setActiveScan(false); // passive advertisements only
  bleScanner->setInterval(160);     // 100 ms units of 0.625 ms
  bleScanner->setWindow(80);        // 50 ms receive window
  cap.begin();
  Serial.println("Callingcard PR5 event pipeline boot");
  drawStatus("READY");
}

void loop() {
  M5Cardputer.update();
  cap.poll();
  pipeline.drainTo(Serial, 8);
  if (static_cast<int32_t>(millis() - nextPipelineLogMs) >= 0) {
    Serial.printf("PIPE queue=%u emitted=%lu dropped=%lu heap=%u\n",
                  pipeline.queued(), static_cast<unsigned long>(pipeline.emitted()),
                  static_cast<unsigned long>(pipeline.dropped()), ESP.getFreeHeap());
    nextPipelineLogMs = millis() + 10000;
  }
  if (static_cast<int32_t>(millis() - nextCapLogMs) >= 0) {
    cap.logStatus();
    nextCapLogMs = millis() + 10000;
  }
  if (static_cast<int32_t>(millis() - nextScanMs) >= 0) {
    ++scanNumber;
    drawStatus(scanWifiNext ? "SCANNING WIFI" : "SCANNING BLE");
    if (scanWifiNext) surveyWifi();
    else surveyBle();
    lastScanEndMs = millis();
    scanWifiNext = !scanWifiNext;
    nextScanMs = lastScanEndMs + kPauseMs;
    drawStatus("SURVEY COMPLETE");
  }
  delay(30);
}
