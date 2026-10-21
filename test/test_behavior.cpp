// Host-side test of HsmBehavior. No hardware is needed.
// Build and run (from stackchan-agent/):
//   g++ -std=c++17 -Wall -Iinclude test/test_behavior.cpp -o test_behavior && ./test_behavior
#include <stdio.h>
#include <stdlib.h>

#include "BatteryMonitor.hpp"
#include "Behavior.hpp"

using namespace stackchan::behavior;

static int failures = 0;
#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
      failures++;                                                    \
    }                                                                \
  } while (0)

static uint32_t t = 0;
static Action step(HsmBehavior& b, uint32_t dt = 33) {
  t += dt;
  Observation o;
  o.now_ms = t;
  return b.update(o);
}
static void run(HsmBehavior& b, uint32_t ms) {
  for (uint32_t i = 0; i < ms; i += 33) step(b);
}

static void test_start_idle() {
  HsmBehavior b;
  Action a = step(b);
  CHECK(b.state() == State::kIdle);
  CHECK(a.set_mood && a.mood == Mood::kNeutral);
}

static void test_touch_smile_then_neutral() {
  HsmBehavior b;
  step(b);
  b.pushEvent(Event(EventType::kTouched));
  Action a = step(b);
  CHECK(b.state() == State::kInteracting);
  CHECK(a.set_mood && a.mood == Mood::kSmile);
  CHECK(a.motion == Motion::kNod);

  run(b, 4000);
  CHECK(b.state() == State::kInteracting);
  bool back = false;
  for (uint32_t i = 0; i < 2000; i += 33) {
    Action x = step(b);
    if (x.set_mood && x.mood == Mood::kNeutral) back = true;
  }
  CHECK(back);
  CHECK(b.state() == State::kIdle);
}

static void test_sleep_and_wake() {
  HsmBehavior b;
  b.sleep_after_ms = 1000;
  step(b);
  run(b, 1500);
  CHECK(b.state() == State::kSleeping);
  b.pushEvent(Event(EventType::kLoudSound));
  step(b);
  CHECK(b.state() == State::kIdle);

  run(b, 1500);
  CHECK(b.state() == State::kSleeping);
  b.pushEvent(Event(EventType::kTouched));
  step(b);
  CHECK(b.state() == State::kInteracting);
}

static void test_attentive() {
  HsmBehavior b;
  step(b);
  b.pushEvent(Event(EventType::kPersonDetected));
  Action a = step(b);
  CHECK(b.state() == State::kAttentive);
  CHECK(a.motion == Motion::kLookFront);
  b.pushEvent(Event(EventType::kPersonLost));
  step(b);
  CHECK(b.state() == State::kIdle);
}

static void test_talking() {
  HsmBehavior b;
  step(b);
  b.pushEvent(Event(EventType::kSpeakRequest, "hello"));
  Action a = step(b);
  CHECK(b.state() == State::kTalking);
  CHECK(a.set_speech && strcmp(a.speech, "hello") == 0);

  // touch does not interrupt the speech
  b.pushEvent(Event(EventType::kTouched));
  step(b);
  CHECK(b.state() == State::kTalking);

  b.pushEvent(Event(EventType::kSpeechFinished));
  a = step(b);
  CHECK(b.state() == State::kIdle);
  CHECK(a.set_speech && strcmp(a.speech, "") == 0);
}

static void test_low_battery_overrides() {
  HsmBehavior b;
  step(b);
  b.pushEvent(Event(EventType::kTouched));
  step(b);
  b.pushEvent(Event(EventType::kBatteryLow));
  Action a = step(b);
  CHECK(b.state() == State::kLowBattery);
  CHECK(a.set_mood && a.mood == Mood::kSad);

  b.pushEvent(Event(EventType::kTouched));  // ignored
  step(b);
  CHECK(b.state() == State::kLowBattery);

  b.pushEvent(Event(EventType::kCharging));
  step(b);
  CHECK(b.state() == State::kIdle);
}

static void test_idle_picker() {
  HsmBehavior b(12345);
  b.sleep_after_ms = 0xFFFFFFFF;
  step(b);
  int motions = 0, smiles = 0;
  for (uint32_t i = 0; i < 10 * 60 * 1000; i += 33) {
    Action a = step(b);
    if (a.motion != Motion::kNone) motions++;
    if (a.set_mood && a.mood == Mood::kSmile) smiles++;
  }
  printf("idle picker in 10 min: %d motions, %d smiles\n", motions, smiles);
  CHECK(motions > 5);
  CHECK(smiles > 0);
  CHECK(b.state() == State::kIdle);
}

static void test_battery_monitor() {
  BatteryMonitor m;
  EventType e = EventType::kTouched;

  // normal level: nothing
  for (int i = 0; i < 20; i++) CHECK(!m.update(80, false, e));

  // low level must persist for debounce_samples
  for (int i = 0; i < m.debounce_samples - 1; i++) CHECK(!m.update(10, false, e));
  CHECK(m.update(10, false, e));
  CHECK(e == EventType::kBatteryLow);
  CHECK(m.isLow());

  // still low: no repeated event
  for (int i = 0; i < 20; i++) CHECK(!m.update(9, false, e));

  // a level between the thresholds without charging does not recover (hysteresis)
  for (int i = 0; i < 20; i++) CHECK(!m.update(20, false, e));
  CHECK(m.isLow());

  // charging recovers after the debounce
  for (int i = 0; i < m.debounce_samples - 1; i++) CHECK(!m.update(12, true, e));
  CHECK(m.update(12, true, e));
  CHECK(e == EventType::kCharging);
  CHECK(!m.isLow());

  // low level while charging is not "low"
  for (int i = 0; i < 20; i++) CHECK(!m.update(5, true, e));

  // a noisy single sample does not trigger
  CHECK(!m.update(5, false, e));
  CHECK(!m.update(50, false, e));
  for (int i = 0; i < m.debounce_samples - 1; i++) CHECK(!m.update(5, false, e));
  CHECK(!m.isLow());

  // invalid level resets the count and is ignored
  CHECK(!m.update(-2, false, e));
  CHECK(!m.update(5, false, e));
  CHECK(!m.isLow());
}

static void test_battery_events_drive_controller() {
  HsmBehavior b;
  BatteryMonitor m;
  step(b);
  EventType e;
  bool low = false;
  for (int i = 0; i < 10 && !low; i++) {
    if (m.update(8, false, e)) {
      b.pushEvent(Event(e));
      low = true;
    }
  }
  CHECK(low);
  Action a = step(b);
  CHECK(b.state() == State::kLowBattery);
  CHECK(a.set_mood && a.mood == Mood::kSad);

  bool charging = false;
  for (int i = 0; i < 10 && !charging; i++) {
    if (m.update(8, true, e)) {
      b.pushEvent(Event(e));
      charging = true;
    }
  }
  CHECK(charging);
  step(b);
  CHECK(b.state() == State::kIdle);
}

int main() {
  test_start_idle();
  test_touch_smile_then_neutral();
  test_sleep_and_wake();
  test_attentive();
  test_talking();
  test_low_battery_overrides();
  test_idle_picker();
  test_battery_monitor();
  test_battery_events_drive_controller();
  if (failures == 0) printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
