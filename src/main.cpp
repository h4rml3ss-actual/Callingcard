// Callingcard PR3: scheduled Wi-Fi survey and BLE advertisement discovery.
// This is a field-observation smoke test, not continuous packet capture.
#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <SPI.h>
#include <SD.h>
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
// Cardputer ADV and Cap LoRa-1262 share SPI CLK=40, MISO=39,
// MOSI=14; microSD CS=12 and SX1262 CS=5. All SPI users run on main
// loop; no SD operations occur in LoRa ISR or BLE callback.
constexpr uint8_t kSdCs = 12;
constexpr uint8_t kLoRaCs = 5;
constexpr uint32_t kFlushEveryMs = 5000;
constexpr uint32_t kMaxSessionBytes = 4UL * 1024 * 1024;
enum class SessionState { NoCard, Recording, Paused, Stopped, Limit, Error };
SessionState sessionState = SessionState::NoCard;
const char* stateName() {
  switch (sessionState) {
    case SessionState::NoCard: return "NO_CARD";
    case SessionState::Recording: return "RECORDING";
    case SessionState::Paused: return "PAUSED";
    case SessionState::Stopped: return "STOPPED";
    case SessionState::Limit: return "LIMIT";
    case SessionState::Error: return "ERROR";
  }
  return "UNKNOWN";
}
File sessionFile;
bool sdMounted = false;
uint32_t sdErrors = 0;
uint32_t lastSessionBytes = 0;
uint32_t nextFlushMs = 0;
uint32_t nextSdLogMs = 0;
uint32_t priorMirrorErrors = 0;
char sessionPath[32] = {};
void closeSession() {
  if (sessionFile) {
    sessionFile.flush();
    lastSessionBytes = sessionFile.size();
    sessionFile.close();
  }
}
void storageError(const char* reason) {
  ++sdErrors;
  Serial.printf("SD ERROR reason=%s\\n", reason);
  closeSession();
  sessionState = SessionState::Error;
}
bool createSession() {
  if (!sdMounted) {
    sessionState = SessionState::NoCard;
    return false;
  }
  if (!SD.exists("/CALLINGCARD") && !SD.mkdir("/CALLINGCARD")) {
    storageError("MKDIR_FAILED");
    return false;
  }
  char candidate[sizeof(sessionPath)] = {};
  bool found = false;
  for (unsigned i = 0; i < 1000; ++i) {
    snprintf(candidate, sizeof(candidate), "/CALLINGCARD/S%03u.JSONL", i);
    if (!SD.exists(candidate)) { found = true; break; }
  }
  if (!found) { storageError("SESSION_DIRECTORY_FULL"); return false; }
  File opened = SD.open(candidate, FILE_WRITE);
  if (!opened) { storageError("OPEN_FAILED"); return false; }
  sessionFile = opened;
  snprintf(sessionPath, sizeof(sessionPath), "%s", candidate);
  lastSessionBytes = 0;
  priorMirrorErrors = pipeline.mirrorErrors();
  nextFlushMs = millis() + kFlushEveryMs;
  sessionState = SessionState::Recording;
  Serial.printf("SD status=RECORDING path=%s\\n", sessionPath);
  return true;
}
void beginStorage() {
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);
  pinMode(kLoRaCs, OUTPUT);
  digitalWrite(kLoRaCs, HIGH);
  if (!SD.begin(kSdCs, SPI, 4000000)) {
    sessionState = SessionState::NoCard;
    Serial.println("SD status=NO_CARD mount_failed=1 serial_only=1");
    return;
  }
  sdMounted = true;
  createSession(); // Preserve PR13 auto-record behavior.
}
void pauseSession() {
  if (sessionState != SessionState::Recording) return;
  sessionFile.flush();
  lastSessionBytes = sessionFile.size();
  sessionState = SessionState::Paused;
  Serial.printf("SD status=PAUSED path=%s bytes=%lu\\n",
                sessionPath, (unsigned long)lastSessionBytes);
}
void resumeSession() {
  if (sessionState != SessionState::Paused) return;
  if (!sessionFile) { storageError("RESUME_FILE_CLOSED"); return; }
  sessionState = SessionState::Recording;
  nextFlushMs = millis() + kFlushEveryMs;
  Serial.printf("SD status=RECORDING path=%s resumed=1\\n", sessionPath);
}
void stopSession() {
  if (sessionState != SessionState::Recording && sessionState != SessionState::Paused) return;
  closeSession();
  sessionState = SessionState::Stopped;
  Serial.printf("SD status=STOPPED path=%s bytes=%lu\\n",
                sessionPath, (unsigned long)lastSessionBytes);
}
void serviceStorage() {
  if (sessionState != SessionState::Recording) return;
  if (pipeline.mirrorErrors() != priorMirrorErrors) {
    priorMirrorErrors = pipeline.mirrorErrors();
    storageError("WRITE_FAILED");
    return;
  }
  if (sessionFile.size() >= kMaxSessionBytes) {
    closeSession();
    sessionState = SessionState::Limit;
    Serial.printf("SD status=LIMIT path=%s bytes=%lu\\n",
                  sessionPath, (unsigned long)lastSessionBytes);
    return;
  }
  if ((int32_t)(millis() - nextFlushMs) >= 0) {
    sessionFile.flush();
    nextFlushMs = millis() + kFlushEveryMs;
  }
}
void handleSessionKeys() {
  // Isolated key-down edges; scanning keeps running through every state.
  static bool priorR = false, priorP = false, priorS = false;
  const bool r = M5Cardputer.Keyboard.isKeyPressed('r');
  const bool p = M5Cardputer.Keyboard.isKeyPressed('p');
  const bool s = M5Cardputer.Keyboard.isKeyPressed('s');
  if (r && !priorR) {
    if (sessionState == SessionState::Paused) resumeSession();
    else if (sessionState == SessionState::Stopped || sessionState == SessionState::Limit)
      createSession();
  }
  if (p && !priorP) {
    if (sessionState == SessionState::Recording) pauseSession();
    else if (sessionState == SessionState::Paused) resumeSession();
  }
  if (s && !priorS) stopSession();
  priorR = r; priorP = p; priorS = s;
}
bool scanWifiNext = true;
BLEScan* bleScanner = nullptr;
callingcard::Observation latest[kMaxLines]{};
int latestCount = 0;

void addObservation(const callingcard::Observation& observation) {
  if (latestCount < kMaxLines) {
    latest[latestCount++] = observation;
  }
  callingcard::Observation enriched = observation;
  cap.annotate(enriched);
  pipeline.enqueue(enriched);
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
            latest[i].kind == callingcard::Kind::WifiNetwork ? 'W' :
            latest[i].kind == callingcard::Kind::BleAdvertisement ? 'B' : 'L',
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
  beginStorage();
  Serial.println("Callingcard PR5 event pipeline boot");
  drawStatus("READY");
}

void loop() {
  M5Cardputer.update();
  handleSessionKeys();
  cap.poll(pipeline);
  pipeline.drainTo(Serial, 8, sessionState == SessionState::Recording ? static_cast<Print*>(&sessionFile) : nullptr);
  serviceStorage();
  if ((int32_t)(millis() - nextSdLogMs) >= 0) {
    Serial.printf("SD status=%s path=%s bytes=%lu errors=%lu\n",
                  stateName(),
                  sessionPath[0] ? sessionPath : "none",
                  sessionFile ? (unsigned long)sessionFile.size() : (unsigned long)lastSessionBytes,
                  (unsigned long)sdErrors);
    nextSdLogMs = millis() + 10000;
  }
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
