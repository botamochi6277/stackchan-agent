// NOTE: add the following definition in SCSerial.cpp to avoid the error
// "error: 'UART_SCLK_DEFAULT' was not declared in this scope"
// #ifndef UART_SCLK_DEFAULT
// #define UART_SCLK_DEFAULT UART_SCLK_APB
// #endif

#include <ArduinoBLE.h>
#include <M5Unified.h>
#include <TaskManager.h>

// stackchan display
#include <ColorPalette.h>
#include <Display.h>
#include <Expression.h>
#include <SpeechBalloon.h>

//

#include "Animation.hpp"
#include "AnimationClipExamples.hpp"
#include "BatteryMonitor.hpp"
#include "Behavior.hpp"
#include "BLEStackchanService.hpp"
#include "PaletteExamples.hpp"

#if defined(ARDUINO_M5Stack_ATOM)
#define RXD 32
#define TXD 26
#elif defined(ARDUINO_M5Stack_Core_ESP32)
// port A
#define RXD 22  // SCL
#define TXD 21  // SDA
#elif defined(ARDUINO_M5STACK_Core2)
#define RXD 33  // SCL
#define TXD 32  // SDA
#elif defined(ARDUINO_XIAO_ESP32C3)
#define RXD 7
#define TXD 6
#elif defined(STACKCHAN_BODY)
#define RXD 7
#define TXD 6
#elif defined(ARDUINO_M5STACK_CORES3)
#define RXD 1
#define TXD 2
#endif

using stackchan::motion::AnimationName;

// the controller returns motions with the same values as AnimationName
static_assert(static_cast<int>(stackchan::behavior::Motion::kNod) ==
                  static_cast<int>(AnimationName::kNod),
              "Motion and AnimationName must have the same values");
static_assert(static_cast<int>(stackchan::behavior::Motion::kShake) ==
                  static_cast<int>(AnimationName::kShake),
              "Motion and AnimationName must have the same values");

Display display;
char balloon_text[20];
// ble::StackchanService stackchan_srv;

#define ANIMATION_FPS 10
stackchan::motion::AnimationController anim_controller(ANIMATION_FPS);
const uint8_t servo_pan_id = 1;
const uint8_t servo_tilt_id = 2;
unsigned short anim_clip_id = 0;

float time_sec = 0.0f;
unsigned long milli_sec = 0U;
unsigned long m5_count = 0;
unsigned long main_count = 0;
stackchan::behavior::HsmBehavior behavior;
stackchan::behavior::BatteryMonitor battery_monitor;
unsigned long last_facial_expression_changed_time = 0;

const Expression expressions[] = {Expression::kNeutral, Expression::kSmile,
                                  Expression::kAngry,   Expression::kSad,
                                  Expression::kSleepy,  Expression::kDoubt};
const uint8_t expressions_size = 6;
Expression current_expression = expressions[0];
uint8_t expression_idx = 0;

const uint8_t NUM_FACES = 4;
Face* faces[NUM_FACES];
uint8_t face_idx = 0;
ColorPalette* color_palettes[NUM_FACES];

void registerFaces();
void changeFace(int8_t delta);
void changeExpression(int8_t delta);
void assignMiaPalette(ColorPalette* palette);
void applyBehaviorAction(const stackchan::behavior::Action& action);
void logTransition(stackchan::behavior::State from,
                   stackchan::behavior::State to, const char* cause);

void setup() {
  // please uncomment if you supply power from GROVE connector to the CORES3
  // #ifdef ARDUINO_M5STACK_CORES3
  //   cfg.output_power = false;
  // #endif

  M5StackChan.begin();

  // M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);

  M5.Lcd.setBrightness(150);
  M5.Lcd.clear();

  M5.Display.drawString("Smart", 10, 10, &fonts::Font4);
  delay(200);
  M5.Display.drawString("Tech", 10, 50, &fonts::Font4);
  delay(200);
  M5.Display.drawString("Animating", 10, 90, &fonts::Font4);
  delay(200);
  M5.Display.drawString("Creative", 10, 130, &fonts::Font4);
  delay(200);
  M5.Display.drawString("Kit", 10, 170, &fonts::Font4);
  delay(200);
  M5.Display.drawString("CHANt", 10, 210, &fonts::Font4);

  delay(3000);

  // default
  faces[0] = display.getFace();
  color_palettes[0] = new ColorPalette();
  // leona
  faces[1] = new stackchan::display::ToonFace2();
  color_palettes[1] = new ColorPalette();
  assignLeonaPalette(color_palettes[1]);
  // ui
  faces[2] = new stackchan::display::ToonFace2();
  color_palettes[2] = new ColorPalette();
  assignUiPalette(color_palettes[2]);
  // fbk
  faces[3] = new stackchan::display::OmegaFace();
  color_palettes[3] = new ColorPalette();
  // assignFbkPalette(color_palettes[3]);
  // pink demon
  // faces[4] = new PinkDemonFace();
  // color_palettes[4] = new ColorPalette();
  // assignPinkDemonPalette(color_palettes[4]);

  display.getCanvas().setColorDepth(8);  // start drawing w/ 8bit color mode
  face_idx = 2;
  display.setColorPalette(color_palettes[face_idx]);
  display.setFace(faces[face_idx]);
  // display.getFace()->autoScale();

  // if (!BLE.begin()) {
  //   // "starting BLE failed!"
  //   display.getSpeechBalloon().setText("BLE is unavailable");
  //   display.update();
  //   M5_LOGW("BLE is unavailable");
  // } else {
  //   M5_LOGD("BLE is available");
  // }

  // display.update();

  // // ## beginning Bluetooth setup
  // String ble_address = BLE.address();
  // String local_name = "Stackchan_" + ble_address;
  // BLE.setDeviceName(local_name.c_str());
  // BLE.setLocalName("Stackchan");
  // BLE.setAdvertisedService(stackchan_srv);

  // // add service
  // BLE.addService(stackchan_srv);
  // stackchan_srv.setInitialValues();
  // // start advertising
  // BLE.advertise();

  // display.update();
  // delay(1000);  // wait for servo to move

  // initial move
  M5StackChan.Motion.goHome(500);

  // register animation clips
  anim_controller.setClip((unsigned short)AnimationName::kLookFront,
                          stackchan::motion::look_front_clip);
  anim_controller.setClip((unsigned short)AnimationName::kLookUp,
                          stackchan::motion::look_up_clip);
  anim_controller.setClip((unsigned short)AnimationName::kLookDown,
                          stackchan::motion::look_down_clip);
  anim_controller.setClip((unsigned short)AnimationName::kLookLeft,
                          stackchan::motion::look_left_clip);
  anim_controller.setClip((unsigned short)AnimationName::kLookRight,
                          stackchan::motion::look_right_clip);
  anim_controller.setClip((unsigned short)AnimationName::kNod,
                          stackchan::motion::nod_clip);
  anim_controller.setClip((unsigned short)AnimationName::kShake,
                          stackchan::motion::head_shake_clip);

  // sprintf(balloon_text, "servo 01 pos: %d",
  //         anim_controller.servo_driver.getCurrentPosition(1));  // 14338???
  // display.getSpeechBalloon().setText(balloon_text);

  M5_LOGI("setting tasks...");
  // ## register tasks
  Tasks.setAutoErase(true);
  Tasks
      .add("M5_update",
           [] {
             M5StackChan.update();
             if (M5.BtnA.wasPressed()) {
               changeFace(1);
             }
             if (M5.BtnB.wasPressed()) {
               changeExpression(1);
             }
             if (M5.BtnC.wasPressed()) {
               anim_controller.play(anim_clip_id);
               anim_clip_id = (anim_clip_id + 1) % 7;
             }

             if (M5.Touch.isEnabled()) {
               auto t = M5.Touch.getDetail();
               if (t.wasFlicked()) {
                 auto dx = t.distanceX();
                 auto dy = t.distanceY();
                 // Flick right/left
                 if (abs(dx) > 50 && abs(dx) > abs(dy)) {
                   if (dx > 0) {
                     changeExpression(1);
                   } else {
                     changeExpression(-1);
                   }
                   last_facial_expression_changed_time = millis();
                 }

                 // Flick up/down
                 if (abs(dy) > 50 && abs(dy) > abs(dx)) {
                   if (dy > 0) {
                     changeFace(-1);
                   } else {
                     changeFace(1);  // change to next face on flick up
                   }
                 }
               }
             }

             auto& head_ts = M5StackChan.TouchSensor;

             if (head_ts.wasClicked()) {
               behavior.pushEvent(stackchan::behavior::Event(
                   stackchan::behavior::EventType::kTouched));
             }

             m5_count += 1;
           })
      ->startFps(100);
  // Tasks
  //     .add("Clock",
  //          [] {
  //            milli_sec = millis();
  //            stackchan_srv.timer_chr.writeValue(milli_sec);
  //          })
  //     ->startFps(60);
  // Tasks
  //     .add("BLE_polling",
  //          [] {
  //            BLE.poll();
  //            stackchan_srv.animationPoll(anim_controller);
  //          })
  //     ->startFps(10);
  Tasks
      .add("Facial_Update",
           [] {
             //  M5.update();
             //  stackchan_srv.facePoll(display, faces, faces_length);
             //  stackchan_srv.facialExpressionPoll(display, expressions,
             //                                     expressions_size);
             //  stackchan_srv.facialColorPoll(display, color_palettes,
             //                                color_palettes_size);
             //  stackchan_srv.mouseOpenPoll(display);
             display.update();
             //  M5_LOGI("Face updated");
           })
      ->startFps(30);

  // the behavior controller decides expressions, speech and motions
  behavior.on_transition = logTransition;
  behavior.seed(micros());
  Tasks
      .add("Behavior_Update",
           [] {
             stackchan::behavior::Observation obs;
             obs.now_ms = millis();
             applyBehaviorAction(behavior.update(obs));
           })
      ->startFps(30);

  // battery level is sampled once per second and turned into BatteryLow / Charging events
  Tasks
      .add("Battery_Monitor",
           [] {
             int level = M5.Power.getBatteryLevel();
             bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
             stackchan::behavior::EventType event;
             if (battery_monitor.update(level, charging, event)) {
               M5_LOGI("battery: %d%% charging=%d -> %s", level, charging,
                       event == stackchan::behavior::EventType::kBatteryLow
                           ? "BatteryLow"
                           : "Charging");
               behavior.pushEvent(stackchan::behavior::Event(event));
             }
           })
      ->startFps(1);

  Tasks
      .add("Animation_Update",
           [] {
             anim_controller.update();
             //  stackchan_srv.servoPoll(anim_controller);
           })
      ->startFps(ANIMATION_FPS);

  display.getSpeechBalloon().setText("");  // hide speech balloon
  anim_controller.play((unsigned short)AnimationName::kNod);
  if (!M5.Touch.isEnabled()) {
    M5_LOGW("Touch is not enabled");
  }
}

void loop() {
  Tasks.update();  // automatically execute tasks

  delay(1);
}

void applyBehaviorAction(const stackchan::behavior::Action& action) {
  using stackchan::behavior::Mood;
  using stackchan::display::Expression;

  if (action.set_mood) {
    Expression e = Expression::kNeutral;
    switch (action.mood) {
      case Mood::kNeutral:
        e = Expression::kNeutral;
        break;
      case Mood::kSmile:
        e = Expression::kSmile;
        break;
      case Mood::kAngry:
        e = Expression::kAngry;
        break;
      case Mood::kSad:
        e = Expression::kSad;
        break;
      case Mood::kSleepy:
        e = Expression::kSleepy;
        break;
      case Mood::kDoubt:
        e = Expression::kDoubt;
        break;
    }
    display.getExpressionWeight().setEmotionalExpression(e, 255);
  }

  if (action.set_speech) {
    display.getSpeechBalloon().setText(action.speech);
  }

  if (action.motion != stackchan::behavior::Motion::kNone) {
    anim_controller.play(static_cast<unsigned short>(action.motion));
  }
}

void logTransition(stackchan::behavior::State from,
                   stackchan::behavior::State to, const char* cause) {
  M5_LOGI("behavior: %s -> %s (%s)", stackchan::behavior::stateName(from),
          stackchan::behavior::stateName(to), cause);
}

void changeFace(int8_t delta) {
  if (delta == 0) {
    return;
  }

  if (delta > 0 && face_idx == NUM_FACES - 1) {
    face_idx = 0;
  } else if (delta < 0 && face_idx == 0) {
    face_idx = NUM_FACES - 1;
  } else {
    face_idx += delta;
  }

  display.setFace(faces[face_idx]);
  display.setColorPalette(color_palettes[face_idx]);
}

void changeExpression(int8_t delta) {
  if (delta == 0) {
    return;
  }

  if (delta > 0 && expression_idx == expressions_size - 1) {
    expression_idx = 0;
  } else if (delta < 0 && expression_idx == 0) {
    expression_idx = expressions_size - 1;
  } else {
    expression_idx += delta;
  }
  display.getExpressionWeight().setEmotionalExpression(
      static_cast<stackchan::display::Expression>(
          expression_idx %
          (static_cast<int>(stackchan::display::Expression::kRelax) + 1)),
      255);
}

void assignMiaPalette(ColorPalette* palette) {
  using stackchan::display::DrawingLocation;
  palette->set(DrawingLocation::kIris1,
               M5.Lcd.color24to16(0x4A99D9));  // iris 1, #4A99D9
  palette->set(DrawingLocation::kEyelash, M5.Lcd.color24to16(0x953346));
  palette->set(DrawingLocation::kEyelid,
               M5.Lcd.color24to16(0x63436F));  // #63436F
  palette->set(DrawingLocation::kIrisBackground, TFT_BLACK);
  palette->set(DrawingLocation::kIris2,
               M5.Lcd.color24to16(0xBFEDE8));  // iris2,#BFEDE8
  palette->set(DrawingLocation::kPupil,
               M5.Lcd.color24to16(0x2D493A));  // #2D493A
  palette->set(DrawingLocation::kEyeHighlight, TFT_WHITE);
  palette->set(DrawingLocation::kSkin,
               M5.Lcd.color24to16(0xfac2a8));  // skin
  palette->set(DrawingLocation::kEyeBrow,
               M5.Lcd.color24to16(0x63434F));  // #63434F
  palette->set(DrawingLocation::kMouthBackground, M5.Lcd.color24to16(0x63434F));
  palette->set(DrawingLocation::kInnerMouth,
               M5.Lcd.color24to16(0x8B0035));  // #8B0035
  palette->set(DrawingLocation::kCheek1,
               M5.Lcd.color24to16(0xd77398));  // cheek #d77398
}
