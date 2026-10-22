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
#include "SessionStats.h"
#include "SessionHistory.h"
#include "HistoryDetails.h"
#include "RfActivity.h"
#include "BladeTelemetry.h"

namespace {
constexpr uint32_t kPauseMs = 2500;
constexpr uint32_t kBleScanSeconds =
#if CALLINGCARD_BLADE_COEXIST
  2;
#else
  4;
#endif
constexpr uint32_t kDisplayRefreshMs = 750;
uint32_t nextDisplayMs = 0;
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
#if CALLINGCARD_BLADE_POC
callingcard::BladeTelemetry blade;
#endif
callingcard::RfActivity activity;
bool activityVisible = false;
callingcard::SessionStats sessionStats;
callingcard::SessionHistory history;
callingcard::HistoryDetails details;
bool detailVisible = false;
bool historyVisible = false;
uint32_t bleLossForSummary = 0;
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
    if (lastSessionBytes == UINT32_MAX) {
      ++sdErrors;
      Serial.println("SD ERROR reason=INVALID_FILE_SIZE value=UINT32_MAX");
      lastSessionBytes = 0;
    }
    sessionFile.close();
  }
}
void storageError(const char* reason) {
  ++sdErrors;
  Serial.printf("SD ERROR reason=%s\\n", reason);
  closeSession();
  sessionState = SessionState::Error;
  if (sessionStats.hasSession()) {
    sessionStats.finish(millis(), pipeline.dropped(), bleLossForSummary);
    sessionStats.print(Serial, millis(), pipeline.dropped(), bleLossForSummary, lastSessionBytes, sessionPath);
  }
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
  sessionStats.begin(millis(), pipeline.dropped(), bleLossForSummary);
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
  sessionStats.finish(millis(), pipeline.dropped(), bleLossForSummary);
  sessionStats.print(Serial, millis(), pipeline.dropped(), bleLossForSummary, lastSessionBytes, sessionPath);
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
  const uint32_t measuredBytes = sessionFile.size();
  if (measuredBytes == UINT32_MAX) {
    storageError("INVALID_FILE_SIZE");
    return;
  }
  if (measuredBytes >= kMaxSessionBytes) {
    closeSession();
    sessionState = SessionState::Limit;
    sessionStats.finish(millis(), pipeline.dropped(), bleLossForSummary);
    sessionStats.print(Serial, millis(), pipeline.dropped(), bleLossForSummary, lastSessionBytes, sessionPath);
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
  static bool priorH = false, priorJ = false, priorK = false, priorD = false, priorA = false;
  const bool r = M5Cardputer.Keyboard.isKeyPressed('r');
  const bool p = M5Cardputer.Keyboard.isKeyPressed('p');
  const bool s = M5Cardputer.Keyboard.isKeyPressed('s');
  const bool h = M5Cardputer.Keyboard.isKeyPressed('h');
  const bool j = M5Cardputer.Keyboard.isKeyPressed('j');
  const bool k = M5Cardputer.Keyboard.isKeyPressed('k');
  const bool d = M5Cardputer.Keyboard.isKeyPressed('d');
  const bool a = M5Cardputer.Keyboard.isKeyPressed('a');
  if (a && !priorA) {
    activityVisible = !activityVisible;
    if (activityVisible) {
      details.close(); detailVisible = false; historyVisible = false;
    }
    nextDisplayMs = 0;
  }
  priorA = a;
  if (h && !priorH) {
    activityVisible = false;
    if (detailVisible) { details.close(); detailVisible = false; }
    else {
      historyVisible = !historyVisible && sdMounted;
      if (historyVisible) history.begin();
    }
    nextDisplayMs = 0;
  }
  if (historyVisible && !detailVisible) {
    if (j && !priorJ) history.move(-1);
    if (k && !priorK) history.move(1);
    if (d && !priorD && !history.scanning()) {
      const auto* selected = history.selected();
      if (selected) {
        detailVisible = true;
        details.open(selected->number);
        nextDisplayMs = 0;
      }
    }
  }
  priorH = h; priorJ = j; priorK = k; priorD = d;
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
enum class ScanMode { Idle, Wifi, Ble };
bool scanWifiNext = true; // Alternate Wi-Fi and BLE scans, starting with Wi-Fi.
ScanMode scanMode = ScanMode::Idle;
uint32_t scanStartedMs = 0;
uint32_t scanTimeouts = 0;
#if CALLINGCARD_BLADE_COEXIST
uint32_t coexSkips = 0;
uint32_t coexScans = 0;
uint32_t nextCoexBleMs = 0;
uint32_t nextCoexLogMs = 0;
#endif
uint32_t bleOverflow = 0;
BLEScan* bleScanner = nullptr;
callingcard::Observation latest[kMaxLines]{};
int latestCount = 0;
// BLE callbacks run in the BLE stack context; never touch EventPipeline,
// Serial, GNSS, or display from that task. Transfer a fixed-size snapshot.
portMUX_TYPE bleMux = portMUX_INITIALIZER_UNLOCKED;
constexpr uint8_t kBleQueueCapacity = 48;
callingcard::Observation bleSamples[kBleQueueCapacity]{};
uint8_t bleHead = 0, bleTail = 0, bleSampleCount = 0;
uint32_t blePrequeueDropped = 0;
callingcard::Observation gnssAnchor{};
uint32_t gnssAnchorMs = 0;
// GNSS anchor refreshed by main loop; callbacks copy without accessing TinyGPSPlus.
void refreshGnssAnchor() {
  callingcard::Observation snapshot{};
  cap.annotate(snapshot);
  portENTER_CRITICAL(&bleMux);
  gnssAnchor = snapshot;
  gnssAnchorMs = millis();
  portEXIT_CRITICAL(&bleMux);
}
void drainBleSamples(uint8_t maximum = 8) {
  for (uint8_t i = 0; i < maximum; ++i) {
    callingcard::Observation snapshot{};
    bool hasSample = false;
    portENTER_CRITICAL(&bleMux);
    if (bleSampleCount) {
      snapshot = bleSamples[bleHead];
      bleHead = (bleHead + 1) % kBleQueueCapacity;
      --bleSampleCount;
      hasSample = true;
    }
    portEXIT_CRITICAL(&bleMux);
    if (!hasSample) return;
    if (latestCount < kMaxLines) latest[latestCount++] = snapshot;
    pipeline.enqueue(snapshot); // Already tagged at callback time, never re-annotate.
  }
}
volatile bool bleDone = false;
volatile uint32_t bleResultCount = 0;
void bleComplete(BLEScanResults results) {
  portENTER_CRITICAL(&bleMux);
  bleResultCount = results.getCount();
  bleDone = true;
  portEXIT_CRITICAL(&bleMux);
}
void addObservation(const callingcard::Observation& observation) {
  if (latestCount < kMaxLines) latest[latestCount++] = observation;
  callingcard::Observation enriched = observation;
  cap.annotate(enriched);
  pipeline.enqueue(enriched);
}
class AdvertisedCallback : public BLEAdvertisedDeviceCallbacks {
 void onResult(BLEAdvertisedDevice device) override {
   callingcard::Observation item{};
   item.kind = callingcard::Kind::BleAdvertisement;
   item.uptimeMs = millis();
   item.rssiDbm = device.getRSSI();
   item.channel = -1;
   const std::string name = device.getName();
   snprintf(item.label, sizeof(item.label), "%s",
            name.empty() ? "<unnamed BLE>" : name.c_str());
   portENTER_CRITICAL(&bleMux);
   ++bleCallbacks;
   // The main loop publishes a recent GNSS snapshot. Do not query TinyGPS
   // or attach scan-completion location to this older advertisement.
   const uint32_t anchorAge = item.uptimeMs - gnssAnchorMs;
   if (anchorAge <= 500) {
     item.hasUtc = gnssAnchor.hasUtc;
     if (item.hasUtc) snprintf(item.utc, sizeof(item.utc), "%s", gnssAnchor.utc);
     item.hasPosition = gnssAnchor.hasPosition;
     if (item.hasPosition) {
       item.latitude = gnssAnchor.latitude;
       item.longitude = gnssAnchor.longitude;
     }
   }
   if (bleSampleCount < kBleQueueCapacity) {
     bleSamples[bleTail] = item;
     bleTail = (bleTail + 1) % kBleQueueCapacity;
     ++bleSampleCount;
   } else {
     ++blePrequeueDropped;
   }
   portEXIT_CRITICAL(&bleMux);
 }
};
AdvertisedCallback callback;

void drawStatus(const char* mode) {
  // Compact 240x135 screen; only redraw on a timer or state change.
  auto& d = M5Cardputer.Display;
  char utc[25] = {};
  const bool hasUtc = cap.gnssUtc(utc, sizeof(utc));
  const uint32_t bytes = sessionFile ? sessionFile.size() : lastSessionBytes;
  const char* file = strrchr(sessionPath, '/');
  file = file ? file + 1 : (sessionPath[0] ? sessionPath : "-");

  d.fillScreen(TFT_BLACK);
  if (activityVisible) {
    d.setTextFont(1);
    d.setTextSize(1);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.setCursor(4, 3);
    d.print("RF ACTIVITY / LAST 2 MIN");
    const uint16_t colors[3] = {TFT_GREEN, TFT_CYAN, TFT_YELLOW};
    const char* names[3] = {"WIFI", "BLE", "LORA"};
    for (uint8_t radio=0; radio<3; ++radio) {
      const int top = 20 + radio*30;
      d.setTextColor(colors[radio], TFT_BLACK);
      d.setCursor(4, top);
      d.printf("%s %lu", names[radio], (unsigned long)activity.sum(radio));
      const uint32_t peak = activity.peak(radio);
      for (uint8_t bin=0; bin<callingcard::RfActivity::kBins; ++bin) {
        const uint32_t count = activity.sample(radio, bin);
        const uint8_t height = peak ? (uint8_t)((uint64_t)count*17/peak) : 0;
        if (height) d.fillRect(76+bin*6, top+19-height, 4, height, colors[radio]);
      }
    }
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawFastHLine(4, 112, 232, TFT_DARKGREY);
    d.setCursor(4, 117);
    d.print("A dashboard  H history");
    return;
  }
  if (historyVisible && detailVisible) {
    d.setTextFont(1);
    d.setTextSize(1);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.setCursor(4, 3);
    d.printf("SESSION S%03u / DETAILS", details.number());
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 20);
    const auto state = details.state();
    if (state == callingcard::HistoryDetails::State::Failed) d.print("READ FAILED");
    else if (state == callingcard::HistoryDetails::State::Done) d.print("COMPLETE");
    else d.printf("READ %lu / %lu", (unsigned long)details.readBytes(),
                  (unsigned long)details.bytes());
    d.setCursor(4, 37);
    d.printf("WIFI %lu BLE %lu LORA %lu",
      (unsigned long)details.observations(0),
      (unsigned long)details.observations(1),
      (unsigned long)details.observations(2));
    d.setCursor(4, 54);
    d.printf("UTC %lu  GPS %lu",
      (unsigned long)details.utcTagged(), (unsigned long)details.positioned());
    d.setCursor(4, 71);
    d.printf("BAD/TRUNC %lu", (unsigned long)details.invalid());
    d.setCursor(4, 88);
    if (details.hasRssi(0)) d.printf("W RSSI %d / %d", details.strongest(0), details.weakest(0));
    else if (details.hasRssi(1)) d.printf("B RSSI %d / %d", details.strongest(1), details.weakest(1));
    d.drawFastHLine(4, 112, 232, TFT_DARKGREY);
    d.setCursor(4, 117);
    d.print("H back");
    return;
  }
  if (historyVisible) {
    d.setTextFont(1);
    d.setTextSize(1);
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.setCursor(4, 3);
    d.print("CALLINGCARD / HISTORY");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 21);
    if (history.scanning()) d.printf("Indexing %u/1000...", history.progress());
    else d.printf("Last %u closed sessions", history.count());
    const auto* entry = history.selected();
    d.setCursor(4, 40);
    if (entry) {
      d.printf("Session S%03u.JSONL", entry->number);
      d.setCursor(4, 57);
      d.printf("File size: %lu bytes", (unsigned long)entry->bytes);
      d.setCursor(4, 74);
      d.printf("Record %u of %u", history.selection()+1, history.count());
    } else d.print(history.scanning() ? "Finding prior recordings..." : "No closed sessions found");
    d.setCursor(4, 94);
    d.printf("Read errors: %lu", (unsigned long)history.errors());
    d.drawFastHLine(4, 112, 232, TFT_DARKGREY);
    d.setCursor(4, 117);
    d.print("J prev K next D details H exit");
    return;
  }
  d.setTextFont(1);
  d.setTextSize(1);
  d.setTextColor(TFT_GREEN, TFT_BLACK);
  d.setCursor(4, 3);
  d.print("CALLINGCARD ADV");
  d.setCursor(165, 3);
  d.print(stateName());
  d.drawFastHLine(4, 15, 232, TFT_DARKGREY);

  d.setTextColor(TFT_WHITE, TFT_BLACK);
  if (sessionState == SessionState::Stopped || sessionState == SessionState::Limit) {
    d.setCursor(4, 21);
    d.printf("SESSION SUMMARY   %lus", (unsigned long)(sessionStats.elapsed(millis())/1000));
    d.setCursor(4, 36);
    d.printf("WIFI %lu  BLE %lu  LORA %lu",
             (unsigned long)sessionStats.count(0),
             (unsigned long)sessionStats.count(1),
             (unsigned long)sessionStats.count(2));
    d.setCursor(4, 51);
    d.printf("SAVED %lu  BYTES %lu",
             (unsigned long)sessionStats.saved(), (unsigned long)lastSessionBytes);
    d.setCursor(4, 66);
    d.printf("PIPE LOST %lu", (unsigned long)pipeline.dropped());
    d.setCursor(4, 81);
    d.printf("FILE %.22s", file);
    d.drawFastHLine(4, 112, 232, TFT_DARKGREY);
    d.setCursor(4, 117);
    d.print("R new session   H history");
    return;
  }
  d.setCursor(4, 21);
  d.printf("WIFI %3lu   BLE %3lu   LORA %4lu",
           (unsigned long)wifiCount, (unsigned long)bleCount,
           (unsigned long)cap.packetCount());
  d.setCursor(4, 34);
  d.printf("GPS: %s   SAT: %lu",
           cap.gnssFix() ? "FIX" : "NO FIX",
           (unsigned long)cap.satellites());
  d.setCursor(4, 47);
  d.printf("UTC: %s", hasUtc ? utc : "--");
  d.setCursor(4, 60);
  d.printf("SD: %.18s", file);
  d.setCursor(4, 73);
  d.printf("DATA: %lu B  SD ERR: %lu",
           (unsigned long)bytes, (unsigned long)sdErrors);
  d.setCursor(4, 86);
  d.printf("PIPE: %u queued / %lu dropped",
           pipeline.queued(), (unsigned long)pipeline.dropped());
  d.setCursor(4, 99);
  d.printf("RADIO: %s  HEAP: %lu",
           cap.radioReady() ? "RX" : "ERR", (unsigned long)ESP.getFreeHeap());
  d.drawFastHLine(4, 112, 232, TFT_DARKGREY);
  d.setCursor(4, 117);
  d.print("P pause S stop R start H hist A RF");
}

void beginSurvey() {
  if (scanMode != ScanMode::Idle) return;
  latestCount = 0;
  scanStartedMs = millis();
  ++scanNumber;
  if (scanWifiNext) {
    const int rc = WiFi.scanNetworks(true, true);
    if (rc == WIFI_SCAN_FAILED) {
      ++errorCount;
      Serial.println("CYCLE mode=wifi start_failed=1");
      nextScanMs = millis() + kPauseMs;
      return;
    }
    scanMode = ScanMode::Wifi;
  } else {
#if CALLINGCARD_BLADE_GATT_ONLY
    Serial.println("BLADE survey_ble_skipped=1 gatt_only=1");
    scanWifiNext = true;
    nextScanMs = millis() + kPauseMs;
    return;
#endif
#if CALLINGCARD_BLADE_COEXIST
    // A connected-first test: do not interrupt advertising while searching for Blade.
    // After connection, allow brief, infrequent survey windows.
    if (!blade.connected() || (int32_t)(millis() - nextCoexBleMs) < 0) {
      ++coexSkips;
      scanWifiNext = true;
      nextScanMs = millis() + kPauseMs;
      return;
    }
    nextCoexBleMs = millis() + 30000;
    ++coexScans;
    Serial.printf("BLADE COEX scan_start=%lu skips=%lu connected=%u\n",
       (unsigned long)coexScans, (unsigned long)coexSkips, blade.connected()?1:0);
#endif
    portENTER_CRITICAL(&bleMux);
    bleDone = false;
    bleHead = bleTail = bleSampleCount = 0;
    bleResultCount = 0;
    bleCallbacks = 0;
    portEXIT_CRITICAL(&bleMux);
    if (!bleScanner->start(kBleScanSeconds, bleComplete, false)) {
      ++errorCount;
      Serial.println("CYCLE mode=ble start_failed=1");
      nextScanMs = millis() + kPauseMs;
      return;
    }
    scanMode = ScanMode::Ble;
  }
  scanWifiNext = !scanWifiNext;
}
void serviceSurvey() {
  if (scanMode == ScanMode::Wifi) {
    const int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) {
      if (millis() - scanStartedMs < 12000) return;
      ++scanTimeouts;
      ++errorCount;
      Serial.println("CYCLE mode=wifi timeout=1");
    } else if (found < 0) {
      ++errorCount;
    } else {
      wifiCount = static_cast<uint32_t>(found);
      for (int i = 0; i < found && latestCount < kMaxLines; ++i) {
        callingcard::Observation item{};
        item.kind = callingcard::Kind::WifiNetwork;
        item.uptimeMs = millis();
        item.rssiDbm = WiFi.RSSI(i);
        item.channel = WiFi.channel(i);
        String ssid = WiFi.SSID(i);
        snprintf(item.label, sizeof(item.label), "%s",
                 ssid.length() ? ssid.c_str() : "<hidden>");
        addObservation(item);
      }
    }
    wifiElapsedMs = millis() - scanStartedMs;
    WiFi.scanDelete();
    Serial.printf("CYCLE mode=wifi count=%lu duration_ms=%lu timeouts=%lu heap=%u errors=%lu\n",
                  (unsigned long)wifiCount, (unsigned long)wifiElapsedMs,
                  (unsigned long)scanTimeouts, ESP.getFreeHeap(), (unsigned long)errorCount);
  } else if (scanMode == ScanMode::Ble) {
    bool complete = false;
    uint32_t total = 0;
    portENTER_CRITICAL(&bleMux);
    complete = bleDone;
    if (complete) {
      total = bleResultCount;
      bleDone = false;
    }
    portEXIT_CRITICAL(&bleMux);
    if (!complete) {
      if (millis() - scanStartedMs < 9000) return;
      ++scanTimeouts;
      ++errorCount;
      bleScanner->stop();
      Serial.println("CYCLE mode=ble timeout=1");
    } else {
      bleCount = total;
      drainBleSamples(kBleQueueCapacity);
    }
    bleElapsedMs = millis() - scanStartedMs;
    bleScanner->clearResults(); // Only after callback completes, in loop context.
    Serial.printf("CYCLE mode=ble unique=%lu callbacks=%lu duration_ms=%lu prequeue_dropped=%lu timeouts=%lu heap=%u errors=%lu\n",
                  (unsigned long)bleCount, (unsigned long)bleCallbacks,
                  (unsigned long)bleElapsedMs, (unsigned long)blePrequeueDropped,
                  (unsigned long)scanTimeouts, ESP.getFreeHeap(), (unsigned long)errorCount);
  } else return;
  scanMode = ScanMode::Idle;
  nextScanMs = millis() + kPauseMs;
}

} // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  BLEDevice::init("Callingcard");
#if CALLINGCARD_BLADE_GATT_ONLY
  Serial.println("BLADE mode=GATT_ONLY ble_survey_disabled=1");
#endif
#if CALLINGCARD_BLADE_POC
  blade.begin();
#endif
  bleScanner = BLEDevice::getScan();
  bleScanner->setAdvertisedDeviceCallbacks(&callback);
  bleScanner->setActiveScan(false); // passive advertisements only
  bleScanner->setInterval(160);     // 100 ms units of 0.625 ms
  bleScanner->setWindow(80);        // 50 ms receive window
  cap.begin();
  pipeline.setActivity(&activity);
  beginStorage();
  Serial.println("Callingcard PR5 event pipeline boot");
  drawStatus("READY");
  nextDisplayMs = millis() + kDisplayRefreshMs;
}

void loop() {
  M5Cardputer.update();
  handleSessionKeys();
  cap.poll(pipeline);
  activity.advance(millis());
  refreshGnssAnchor();
  drainBleSamples();
  bleLossForSummary = blePrequeueDropped;
  pipeline.drainTo(Serial, 8, sessionState == SessionState::Recording ? static_cast<Print*>(&sessionFile) : nullptr,
                   sessionStats.hasSession() && (sessionState == SessionState::Recording || sessionState == SessionState::Paused) ? &sessionStats : nullptr);
  serviceStorage();
#if CALLINGCARD_BLADE_POC
  blade.tick(wifiCount, bleCount, cap.packetCount(), cap.gnssFix(),
             sessionState == SessionState::Recording);
#endif
  if (historyVisible && sdMounted && !detailVisible) history.tick(sessionFile ? sessionPath : nullptr);
  if (detailVisible) details.tick();
  if ((int32_t)(millis() - nextDisplayMs) >= 0) {
    drawStatus("STATUS");
    nextDisplayMs = millis() + kDisplayRefreshMs;
  }
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
  serviceSurvey();
#if CALLINGCARD_BLADE_COEXIST
  if ((int32_t)(millis() - nextCoexLogMs) >= 0) {
    Serial.printf("BLADE COEX connected=%u ble_scans=%lu skipped=%lu dropped=%lu prequeue=%lu\n",
      blade.connected()?1:0,(unsigned long)coexScans,(unsigned long)coexSkips,
      (unsigned long)pipeline.dropped(),(unsigned long)blePrequeueDropped);
    nextCoexLogMs = millis() + 10000;
  }
#endif
  if (scanMode == ScanMode::Idle &&
      static_cast<int32_t>(millis() - nextScanMs) >= 0) beginSurvey();
  delay(10);
}
