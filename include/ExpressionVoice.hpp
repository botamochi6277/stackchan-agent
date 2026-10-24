#pragma once

#include <Expression.h>
#include <M5Unified.h>
#include <StackChanVoice.h>

namespace stackchan::behavior {

// Emotion (stackchan-voice) and Expression (stackchan-display) have the same order,
// so one can be converted to the other with static_cast.
#define STACKCHAN_ASSERT_SAME_EMOTION(name)                                   \
  static_assert(static_cast<int>(stackchan::voice::Emotion::name) ==          \
                    static_cast<int>(stackchan::display::Expression::name),   \
                "Emotion and Expression must have the same values")
STACKCHAN_ASSERT_SAME_EMOTION(kNeutral);
STACKCHAN_ASSERT_SAME_EMOTION(kAngry);
STACKCHAN_ASSERT_SAME_EMOTION(kSad);
STACKCHAN_ASSERT_SAME_EMOTION(kDoubt);
STACKCHAN_ASSERT_SAME_EMOTION(kSleepy);
STACKCHAN_ASSERT_SAME_EMOTION(kSmile);
STACKCHAN_ASSERT_SAME_EMOTION(kGrin);
STACKCHAN_ASSERT_SAME_EMOTION(kSurprised);
STACKCHAN_ASSERT_SAME_EMOTION(kUpset);
STACKCHAN_ASSERT_SAME_EMOTION(kRelax);
#undef STACKCHAN_ASSERT_SAME_EMOTION

/**
 * @brief Plays the voice of a character when the facial expression changes.
 *
 * The speaker and the microphone share the I2S port of the CoreS3. The speaker
 * is started only while a voice is played, and the microphone is stopped for
 * that time and started again after it. Check busy() before reading the
 * microphone.
 *
 * Call onExpression() when an expression is set, and update() periodically.
 */
class ExpressionVoice {
 public:
  /// character whose voice is played
  stackchan::voice::Character character = stackchan::voice::Character::kRobot;
  /// speaker volume, 0 to 255
  uint8_t volume = 128;
  /// whether going back to the neutral face makes a sound
  bool voice_neutral = false;

  /**
   * @param mic_enabled true if the microphone is running, so it is stopped
   *        while the voice is played and restarted afterwards
   */
  void begin(bool mic_enabled) { mic_enabled_ = mic_enabled; }

  /// @return true while the speaker is in use. The microphone is stopped.
  bool busy() const { return speaker_active_; }

  /// Play the voice for the expression, if it differs from the last one.
  void onExpression(stackchan::display::Expression expression) {
    if (expression == last_expression_) {
      return;
    }
    last_expression_ = expression;
    if (!voice_neutral && expression == stackchan::display::Expression::kNeutral) {
      return;
    }
    if (static_cast<int>(expression) >=
        static_cast<int>(stackchan::voice::Emotion::kCount)) {
      return;  // not an emotion (blink, blush, ...)
    }

    stackchan::voice::VoiceNote notes[stackchan::voice::kMaxEmotionNotes];
    size_t count = stackchan::voice::emotionVoice(
        character, static_cast<stackchan::voice::Emotion>(expression), notes,
        stackchan::voice::kMaxEmotionNotes, micros());
    if (count == 0) {
      return;
    }

    if (!speaker_active_) {
      if (mic_enabled_) {
        M5.Mic.end();
      }
      if (!player_.begin()) {
        M5_LOGW("speaker is not available");
        restoreMic();
        return;
      }
      speaker_active_ = true;
    }
    player_.setVolume(volume);
    // a new voice replaces the one that is still playing
    if (!player_.playPhrase(notes, count)) {
      M5_LOGW("voice was not played");
    }
  }

  /// Give the I2S port back to the microphone when the voice has finished.
  void update() {
    if (speaker_active_ && !player_.isPlaying()) {
      M5.Speaker.end();
      speaker_active_ = false;
      restoreMic();
    }
  }

 private:
  void restoreMic() {
    if (mic_enabled_ && !M5.Mic.begin()) {
      M5_LOGW("microphone could not be restarted");
    }
  }

  stackchan::voice::VoicePlayer player_;
  stackchan::display::Expression last_expression_ =
      stackchan::display::Expression::kNeutral;
  bool speaker_active_ = false;
  bool mic_enabled_ = false;
};

}  // namespace stackchan::behavior
