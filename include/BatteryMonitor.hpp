// Turns battery samples into BatteryLow / Charging events for the behavior controller.
// Like Behavior.hpp, it has no dependency on Arduino / M5Unified so that it can be tested on a PC.
#pragma once

#include <stdint.h>

#include "Behavior.hpp"

namespace stackchan::behavior {

class BatteryMonitor {
 public:
  // thresholds in percent. recover_percent > low_percent gives hysteresis,
  // so the state does not flip back and forth around the threshold.
  int low_percent = 15;
  int recover_percent = 25;
  // number of consecutive samples that must agree before an event is emitted
  // (battery readings are noisy). With one sample per second, this is in seconds.
  uint8_t debounce_samples = 5;

  /**
   * @brief Feed one battery sample
   *
   * @param level_percent battery level 0..100. Values out of this range (no battery, read error) are ignored.
   * @param charging true while the battery is charging
   * @param event [out] the event to push to the controller when this function returns true
   * @return true if an event should be emitted
   */
  bool update(int level_percent, bool charging, EventType& event) {
    if (level_percent < 0 || level_percent > 100) {
      count_ = 0;
      return false;
    }

    bool condition = is_low_ ? (charging || level_percent >= recover_percent)
                             : (!charging && level_percent <= low_percent);
    if (!condition) {
      count_ = 0;
      return false;
    }

    if (++count_ < debounce_samples) {
      return false;
    }

    count_ = 0;
    is_low_ = !is_low_;
    event = is_low_ ? EventType::kBatteryLow : EventType::kCharging;
    return true;
  }

  /// @return true while the battery is considered low
  bool isLow() const { return is_low_; }

 private:
  bool is_low_ = false;
  uint8_t count_ = 0;
};

}  // namespace stackchan::behavior
