// Callingcard Cardputer ADV hardware baseline.
// Intentional scope: local display + bounded 2.4 GHz Wi-Fi SSID survey.
// BLE, GNSS, LoRa, SD and HUD remain separate milestones.
#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>

namespace {
constexpr uint32_t kScanPeriodMs = 10000;
constexpr int kMaxDisplayed = 5;
uint32_t nextScanMs = 0;
uint32_t scanNumber = 0;

void drawHeader(const char* state) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setCursor(4, 3);
  d.setTextSize(1);
  d.setTextColor(TFT_GREEN, TFT_BLACK);
  d.println("CALLINGCARD / ADV");
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.printf("%s   #%lu\n", state, static_cast<unsigned long>(scanNumber));
}

void survey() {
  ++scanNumber;
  drawHeader("SCANNING");
  // Blocking survey deliberately chosen for the first hardware smoke test.
  // Do not interpret counts as continuous packet monitoring.
  int count = WiFi.scanNetworks(false, true);
  drawHeader(count < 0 ? "SCAN ERROR" : "WIFI SURVEY");
  auto& d = M5Cardputer.Display;
  d.printf("APs: %d   Heap: %u\n", count < 0 ? 0 : count, ESP.getFreeHeap());
  if (count > 0) {
    int limit = count < kMaxDisplayed ? count : kMaxDisplayed;
    for (int i = 0; i < limit; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) ssid = "<hidden>";
      // Display truncation is intentional; avoid variable-sized buffers.
      if (ssid.length() > 19) ssid = ssid.substring(0, 19);
      d.printf("%2d %-19s %4d\n", i + 1, ssid.c_str(), WiFi.RSSI(i));
    }
  }
  d.println("2.4GHz only | no logging");
  Serial.printf("survey=%lu networks=%d heap=%u\n",
                static_cast<unsigned long>(scanNumber), count, ESP.getFreeHeap());
  WiFi.scanDelete();
  nextScanMs = millis() + kScanPeriodMs;
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
  drawHeader("BOOT");
  Serial.println("Callingcard ADV Wi-Fi baseline boot");
}

void loop() {
  M5Cardputer.update();
  if (static_cast<int32_t>(millis() - nextScanMs) >= 0) survey();
  delay(30);
}
