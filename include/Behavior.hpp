// Behavior controller of StackChan: a hierarchical state machine with an idle behavior picker.
// See ../behavior.md for the design.
//
// This header has no dependency on Arduino / M5Unified so that it can be built and tested on a PC.
#pragma once

#include <stdint.h>
#include <string.h>

namespace stackchan::behavior {

/// Emotional expression requested by the controller. Mapped to display expressions by the caller.
enum class Mood : uint8_t {
  kNeutral = 0,
  kSmile,
  kAngry,
  kSad,
  kSleepy,
  kDoubt,
  kSurprised,
};

/// Head motion requested by the controller.
/// Values are the same as stackchan::motion::AnimationName (kNone means no motion).
enum class Motion : int8_t {
  kNone = -1,
  kLookFront = 0,
  kLookUp,
  kLookDown,
  kLookLeft,
  kLookRight,
  kNod,
  kShake,
};

enum class EventType : uint8_t {
  kTouched,         // head touch sensor was clicked
  kPersonDetected,  // someone is in front of StackChan
  kPersonLost,
  kLoudSound,
  kBatteryLow,
  kCharging,
  kSpeakRequest,    // show `text` in the speech balloon
  kSpeechFinished,  // finish talking before the timeout
};

constexpr size_t kMaxTextLength = 47;

struct Event {
  EventType type;
  char text[kMaxTextLength + 1] = {0};  // used by kSpeakRequest

  Event(EventType t = EventType::kTouched) : type(t) {}
  Event(EventType t, const char* s) : type(t) {
    strncpy(text, s, kMaxTextLength);
    text[kMaxTextLength] = '\0';
  }
};

struct Observation {
  uint32_t now_ms = 0;
};

/// What the controller wants to change. Only the fields with `set_*` / motion != kNone should be applied.
struct Action {
  bool set_mood = false;
  Mood mood = Mood::kNeutral;
  bool set_speech = false;
  const char* speech = "";  // "" hides the speech balloon. Valid until the next update().
  Motion motion = Motion::kNone;

  bool empty() const { return !set_mood && !set_speech && motion == Motion::kNone; }
};

enum class State : uint8_t {
  // children of Awake
  kIdle = 0,
  kAttentive,
  kInteracting,
  kTalking,
  // others
  kSleeping,
  kLowBattery,
};

inline const char* stateName(State s) {
  switch (s) {
    case State::kIdle:
      return "Idle";
    case State::kAttentive:
      return "Attentive";
    case State::kInteracting:
      return "Interacting";
    case State::kTalking:
      return "Talking";
    case State::kSleeping:
      return "Sleeping";
    case State::kLowBattery:
      return "LowBattery";
  }
  return "?";
}

/// Awake is the parent state of Idle, Attentive, Interacting and Talking.
inline bool isAwake(State s) {
  return s == State::kIdle || s == State::kAttentive ||
         s == State::kInteracting || s == State::kTalking;
}

/// Callback to log transitions.
using TransitionCallback = void (*)(State from, State to, const char* cause);

class HsmBehavior {
 public:
  // timings in milliseconds. They are public to be tuned.
  uint32_t interacting_ms = 5000;
  uint32_t attentive_timeout_ms = 15000;
  uint32_t sleep_after_ms = 5 * 60 * 1000;
  uint32_t pick_min_ms = 3000;
  uint32_t pick_max_ms = 10000;
  uint32_t transient_ms = 2000;  // duration of an idle smile
  uint32_t surprised_ms = 1500;  // duration of the surprised face after a loud sound

  TransitionCallback on_transition = nullptr;

  explicit HsmBehavior(uint32_t seed = 1) : rng_(seed ? seed : 1) {}

  /// Seed the random number generator of the idle picker.
  void seed(uint32_t seed) { rng_ = seed ? seed : 1; }

  State state() const { return state_; }

  /// Queue an event. It is handled in the next update(). Events are dropped if the queue is full.
  void pushEvent(const Event& e) {
    if (queue_len_ < kQueueSize) {
      queue_[(queue_head_ + queue_len_) % kQueueSize] = e;
      queue_len_++;
    }
  }

  /// Handle events and timers, and return what to change.
  Action update(const Observation& obs) {
    now_ = obs.now_ms;
    action_ = Action();

    if (!started_) {
      started_ = true;
      last_activity_ms_ = now_;
      enter(State::kIdle, "start");
    }

    while (queue_len_ > 0) {
      Event e = queue_[queue_head_];
      queue_head_ = (queue_head_ + 1) % kQueueSize;
      queue_len_--;
      handle(e);
    }

    updateState();
    return action_;
  }

 private:
  static constexpr uint8_t kQueueSize = 8;

  State state_ = State::kIdle;
  bool started_ = false;
  uint32_t now_ = 0;
  uint32_t entered_ms_ = 0;
  uint32_t last_activity_ms_ = 0;
  uint32_t talk_ms_ = 0;       // duration of the current speech
  uint32_t next_pick_ms_ = 0;  // idle picker
  uint32_t transient_until_ms_ = 0;
  uint32_t rng_;
  char text_[kMaxTextLength + 1] = {0};
  Action action_;  // accumulated during one update()

  Event queue_[kQueueSize];
  uint8_t queue_head_ = 0;
  uint8_t queue_len_ = 0;

  // idle candidates: weight and cooldown
  enum Candidate : uint8_t { kLook = 0, kNodCand, kSmileCand, kNothing, kNumCandidates };
  static constexpr uint8_t kWeights[kNumCandidates] = {3, 1, 1, 5};
  static constexpr uint32_t kCooldowns[kNumCandidates] = {10000, 30000, 30000, 0};
  uint32_t last_picked_ms_[kNumCandidates] = {0, 0, 0, 0};
  bool picked_before_[kNumCandidates] = {false, false, false, false};

  uint32_t random(uint32_t n) {  // xorshift32, 0..n-1
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return n == 0 ? 0 : rng_ % n;
  }

  void setMood(Mood m) {
    action_.set_mood = true;
    action_.mood = m;
  }

  void setSpeech(const char* s) {
    action_.set_speech = true;
    action_.speech = s;
  }

  void startle() {
    setMood(Mood::kSurprised);
    transient_until_ms_ = now_ + surprised_ms;  // back to neutral by updateTransientMood()
  }

  void scheduleNextPick() {
    next_pick_ms_ = now_ + pick_min_ms + random(pick_max_ms - pick_min_ms + 1);
  }

  void transition(State to, const char* cause) {
    State from = state_;
    // onExit
    if (from == State::kTalking) {
      setSpeech("");
    }
    if (on_transition) {
      on_transition(from, to, cause);
    }
    enter(to, cause);
  }

  void enter(State s, const char* /*cause*/) {
    state_ = s;
    entered_ms_ = now_;
    transient_until_ms_ = 0;
    switch (s) {
      case State::kIdle:
        setMood(Mood::kNeutral);
        scheduleNextPick();
        break;
      case State::kAttentive:
        setMood(Mood::kNeutral);
        action_.motion = Motion::kLookFront;
        scheduleNextPick();
        break;
      case State::kInteracting:
        setMood(Mood::kSmile);
        action_.motion = Motion::kNod;
        break;
      case State::kTalking:
        setMood(Mood::kNeutral);
        setSpeech(text_);
        break;
      case State::kSleeping:
        setMood(Mood::kSleepy);
        action_.motion = Motion::kLookDown;
        break;
      case State::kLowBattery:
        setMood(Mood::kSad);
        break;
    }
  }

  void handle(const Event& e) {
    // transitions defined on the parent state (Awake) and on the top level
    if (e.type == EventType::kBatteryLow) {
      if (state_ != State::kLowBattery) {
        transition(State::kLowBattery, "battery low");
      }
      return;
    }

    if (state_ == State::kLowBattery) {
      if (e.type == EventType::kCharging) {
        last_activity_ms_ = now_;
        transition(State::kIdle, "charging");
      }
      return;  // everything else is ignored while the battery is low
    }

    last_activity_ms_ = now_;

    switch (e.type) {
      case EventType::kTouched:
        if (state_ == State::kTalking) {
          break;  // do not interrupt the speech
        }
        if (state_ == State::kInteracting) {
          // touched again: restart the timer and the nod
          entered_ms_ = now_;
          action_.motion = Motion::kNod;
          break;
        }
        transition(State::kInteracting, "touched");
        break;

      case EventType::kPersonDetected:
        if (state_ == State::kIdle) {
          transition(State::kAttentive, "person detected");
        } else if (state_ == State::kAttentive) {
          entered_ms_ = now_;  // keep attentive
        }
        break;

      case EventType::kPersonLost:
        if (state_ == State::kAttentive) {
          transition(State::kIdle, "person lost");
        }
        break;

      case EventType::kLoudSound:
        // startled: wake up if sleeping, and show a surprised face for a moment.
        // Other states (touched, talking) keep their own face.
        if (state_ == State::kSleeping) {
          transition(State::kIdle, "loud sound");
          startle();
        } else if (state_ == State::kIdle || state_ == State::kAttentive) {
          startle();
        }
        break;

      case EventType::kSpeakRequest:
        strncpy(text_, e.text, kMaxTextLength);
        text_[kMaxTextLength] = '\0';
        talk_ms_ = 2000 + 80 * strlen(text_);
        if (talk_ms_ > 15000) talk_ms_ = 15000;
        if (state_ == State::kTalking) {
          // replace the text
          entered_ms_ = now_;
          setSpeech(text_);
        } else {
          transition(State::kTalking, "speak request");
        }
        break;

      case EventType::kSpeechFinished:
        if (state_ == State::kTalking) {
          transition(State::kIdle, "speech finished");
        }
        break;

      default:
        break;
    }
  }

  void updateState() {
    uint32_t elapsed = now_ - entered_ms_;
    switch (state_) {
      case State::kIdle:
        if (now_ - last_activity_ms_ > sleep_after_ms) {
          transition(State::kSleeping, "no activity");
          return;
        }
        updateTransientMood();
        pickIdleBehavior();
        break;

      case State::kAttentive:
        if (elapsed > attentive_timeout_ms) {
          transition(State::kIdle, "attentive timeout");
          return;
        }
        updateTransientMood();
        break;

      case State::kInteracting:
        if (elapsed > interacting_ms) {
          transition(State::kIdle, "interacting timeout");
        }
        break;

      case State::kTalking:
        if (elapsed > talk_ms_) {
          transition(State::kIdle, "talking timeout");
        }
        break;

      case State::kSleeping:
      case State::kLowBattery:
        break;
    }
  }

  void updateTransientMood() {
    if (transient_until_ms_ != 0 && now_ >= transient_until_ms_) {
      transient_until_ms_ = 0;
      setMood(Mood::kNeutral);
    }
  }

  void pickIdleBehavior() {
    if (now_ < next_pick_ms_) {
      return;
    }
    scheduleNextPick();

    // weighted random choice among the candidates out of cooldown
    uint32_t total = 0;
    bool eligible[kNumCandidates];
    for (uint8_t i = 0; i < kNumCandidates; i++) {
      eligible[i] = !picked_before_[i] ||
                    (now_ - last_picked_ms_[i] >= kCooldowns[i]);
      if (eligible[i]) total += kWeights[i];
    }
    if (total == 0) return;

    uint32_t r = random(total);
    uint8_t chosen = kNothing;
    for (uint8_t i = 0; i < kNumCandidates; i++) {
      if (!eligible[i]) continue;
      if (r < kWeights[i]) {
        chosen = i;
        break;
      }
      r -= kWeights[i];
    }

    if (chosen != kNothing) {
      picked_before_[chosen] = true;
      last_picked_ms_[chosen] = now_;
    }

    switch (chosen) {
      case kLook: {
        static const Motion looks[3] = {Motion::kLookLeft, Motion::kLookRight,
                                        Motion::kLookUp};
        action_.motion = looks[random(3)];
        break;
      }
      case kNodCand:
        action_.motion = Motion::kNod;
        break;
      case kSmileCand:
        setMood(Mood::kSmile);
        transient_until_ms_ = now_ + transient_ms;
        break;
      default:
        break;
    }
  }
};

}  // namespace stackchan::behavior
