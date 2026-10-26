#pragma once
namespace callingcard {
// Integrated survey order: active Wi-Fi -> short passive monitor -> BLE.
// Preserve existing BLE coexistence gating; passive never overlaps Wi-Fi scan.
struct IntegratedWifiSchedule {
  bool pendingPassive=false;
  void onActiveWifiFinished() { pendingPassive=true; }
  bool shouldStartPassive() const { return pendingPassive; }
  void onPassiveFinished() { pendingPassive=false; }
  void reset() { pendingPassive=false; }
};
} // namespace callingcard
