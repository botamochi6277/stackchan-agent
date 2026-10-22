// Detects loud sounds (claps, knocks, shouts) in microphone samples and turns them into LoudSound events.
// Like Behavior.hpp, it has no dependency on Arduino / M5Unified so that it can be tested on a PC.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "Behavior.hpp"

namespace stackchan::behavior {

class SoundMonitor {
 public:
  // A chunk is loud when its RMS is above both of
  //  - min_rms: absolute level on the int16 scale (0..32767),
  //  - noise_floor * ratio: relative to the slowly adapting level of the room noise,
  // so a steady loud environment does not trigger the event all the time.
  float min_rms = 3000.0f;
  float ratio = 6.0f;
  // minimum time between two events [msec]
  uint32_t cooldown_ms = 3000;

  /**
   * @brief Feed one chunk of microphone samples
   *
   * @param samples mono int16 samples
   * @param length number of samples
   * @param now_ms current time [msec]
   * @param event [out] the event to push to the controller when this function returns true
   * @return true if an event should be emitted
   */
  bool update(const int16_t* samples, size_t length, uint32_t now_ms, EventType& event) {
    if (samples == nullptr || length == 0) {
      return false;
    }

    float rms = computeRms(samples, length);

    if (!initialized_) {
      initialized_ = true;
      noise_floor_ = rms;
      return false;  // the first chunk only sets the noise floor
    }

    bool loud = rms > min_rms && rms > noise_floor_ * ratio;

    // the noise floor follows quiet sounds quickly and loud sounds very slowly
    float rate = loud ? 0.002f : 0.05f;
    noise_floor_ += (rms - noise_floor_) * rate;

    if (!loud) {
      return false;
    }
    if (has_fired_ && (now_ms - last_fired_ms_) < cooldown_ms) {
      return false;
    }

    has_fired_ = true;
    last_fired_ms_ = now_ms;
    event = EventType::kLoudSound;
    return true;
  }

  float noiseFloor() const { return noise_floor_; }

  /// RMS of the samples after removing the DC offset of the microphone.
  static float computeRms(const int16_t* samples, size_t length) {
    double mean = 0.0;
    for (size_t i = 0; i < length; i++) {
      mean += samples[i];
    }
    mean /= static_cast<double>(length);

    double sum = 0.0;
    for (size_t i = 0; i < length; i++) {
      double d = samples[i] - mean;
      sum += d * d;
    }
    return static_cast<float>(sqrt(sum / static_cast<double>(length)));
  }

 private:
  bool initialized_ = false;
  bool has_fired_ = false;
  float noise_floor_ = 0.0f;
  uint32_t last_fired_ms_ = 0;
};

}  // namespace stackchan::behavior
