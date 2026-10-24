# Stackchan Agent

[![pioarduino Build](https://github.com/botamochi6277/stackchan-agent/actions/workflows/pioarduino-ci.yml/badge.svg?branch=main)](https://github.com/botamochi6277/stackchan-agent/actions/workflows/pioarduino-ci.yml)

*stackchan-agent* is the central controller for the **next generation** of stackchan.
It decides how StackChan behaves, like an NPC, and drives the other modules:

- [stackchan-display](https://github.com/botamochi6277/stackchan-display) is a library to control facial expression.
- [stackchan-voice](https://github.com/botamochi6277/stackchan-voice) is a library to play sounds. StackChan makes the voice of a character when the facial expression changes (see [Voice](#voice)).
- *motion control* (`include/Animation.hpp`) is a sub library of this repository to control servo motions and animations.
- [StackChan-BSP](https://github.com/botamochi6277/StackChan-BSP) is the board support package of the StackChan body (servos, head touch sensor, LEDs).
- [M5Unified](https://github.com/m5stack/M5Unified) is the hardware abstraction layer of M5Stack devices. It is a relatively low-level API that gives the same way to use the devices of different boards (CoreS3, Core2, Core, ...), and the other libraries (stackchan-display, stackchan-voice, StackChan-BSP) are built on it. This repository uses it to operate:
  - the LCD (`M5.Display` / `M5.Lcd`, drawing is done by M5GFX) and the touch screen (`M5.Touch`)
  - the buttons (`M5.BtnA/B/C`) of the boards that have them
  - the microphone (`M5.Mic`, for the loud sound detection) and the speaker (`M5.Speaker`, played by stackchan-voice)
  - the power management (`M5.Power`, for the battery level and the charging state)

```mermaid
---
config:
  theme: 'redux-dark'
title: 'StackChan Firmware Architecture'
---
graph LR

subgraph agent[stackchan-agent]
    policy(("policy/\nbehavior controller"))
end

style agent stroke:#2CFF05,stroke-width:4px

%% sensor inputs

subgraph sensor
    camera
    touch
    mic
end

camera[fas:fa-camera camera]-->|cam image|agent
touch[head touch sensor]-->agent
mic[microphone]-->agent

subgraph state
    head_rot
    bat
end

head_rot[head rotation sensor]-->|pan tilt angles|agent
bat[battery sensor]-->agent

%% comm inputs
comm_proxy[com. proxy]
esp_now[esp now]-.->comm_proxy
ble[bluetooth]-.->comm_proxy
usb[usb]-.->comm_proxy
wifi[WiFi]-.->comm_proxy
comm_proxy-->agent

%% extra

%% llm[large language model]-.->wifi

%% outputs

agent-->display

subgraph display[stackchan-display]
    face([face])
    speech([speech bubble])
    decorators([decorators])
end

display-->display_lib["display"]

agent-->mot[motion control]
mot-->servo_lib[servo]

%% head neopixels on M5StackChan Head
head_led[head LED control]
agent-->head_led[head LED control]
head_led-->led_lib[LED]

subgraph stackchan-voice
  voice[voice control]
end
agent-->voice-->speaker

%% custom dependencies developed by botamochi
classDef  custom_lib stroke:#59ffff,stroke-width:4px
class display,stackchan-voice custom_lib

```

## Features

- **Behavior controller**: a hierarchical state machine (`Idle`, `Attentive`, `Interacting`, `Talking`, `Sleeping`, `LowBattery`) with an idle picker that makes StackChan look around, nod and smile on its own.
  Touching the head makes it smile and nod, and it falls asleep after 5 minutes without events. See [behavior.md](behavior.md).
- **Face and expression** with [stackchan-display](https://github.com/botamochi6277/stackchan-display). Buttons A/B change the face and the expression, and flicking the screen changes them too (manual debug control).
- **Voice** with [stackchan-voice](https://github.com/botamochi6277/stackchan-voice). A change of the expression plays a voice of the character (robot, cat, dog, bird, owl or friendly robot) that suits the emotion. The speaker and the microphone share the I2S port, so the microphone is stopped while the voice is played. See [Voice](#voice).
- **Head motion** with animation clips (look front / up / down / left / right, nod, shake). Button C plays the clips in order. See [animation.md](animation.md).
- **BLE service** to control the face and servos from a central device. The BLE code is currently commented out in `src/main.cpp`.

The head touch sensor, the battery level (low battery -> sad face until charging) and the microphone (a loud sound makes StackChan look surprised, and wakes it up) are connected to the behavior controller. Camera and BLE / LLM inputs are not connected yet.
See the plan in [behavior.md](behavior.md) and the overall design in [policy.md](policy.md).

## Hardware

Which device the ESP32-S3 talks to by SPI / I2C / I2S / UART, with the pins and the I2C addresses, is in [hardware.md](hardware.md). Check it before adding a peripheral.

## Build

Install [PlatformIO](https://platformio.org/) (or the [pioarduino](https://marketplace.visualstudio.com/items?itemName=pioarduino.pioarduino-ide) VSCode extension), then run in this directory:

```bash
pio run -e m5stack-cores3-stackchan-body              # build (default env)
pio run -e m5stack-cores3-stackchan-body -t upload    # upload
pio device monitor                                    # 115200 baud
```

| env | board |
| --- | --- |
| `m5stack-cores3-stackchan-body` | CoreS3 on the StackChan body (default) |
| `m5stack-cores3-stackchan-body-pioarduino` | the same as above, built with [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino-ESP32 3.x / ESP-IDF 5.x). This is what the CI builds. |
| `m5stack-cores3` | M5Stack CoreS3 |
| `m5stack-core2` | M5Stack Core2 |
| `m5stack-core1` | M5Stack Core (Basic / Gray) |

Notes on dependencies:

- `M5Unified` is pinned to `0.2.26`, and `StackChan-BSP` is taken from the fork `botamochi6277/StackChan-BSP`, because `StackChan-BSP` 1.1.0 from the registry does not compile with recent `M5Unified`.
- `stackchan-display` is pinned to the tag `0.5.0`, and `stackchan-voice` to the tag `0.1.0`.

## Voice

When the facial expression changes, StackChan plays the voice of a character with [stackchan-voice](https://github.com/botamochi6277/stackchan-voice) (`include/ExpressionVoice.hpp`).

- The expression is set through `setExpression()` in `src/main.cpp`, so a change by the behavior controller, a button or a flick makes a sound.
- The character (`kRobot` by default), the volume and whether returning to the neutral face makes a sound (off by default) are the members of `ExpressionVoice`.
- The speaker and the microphone share the I2S port of the CoreS3. The microphone is stopped while a voice is played and started again after it, so the loud sound detection does not hear StackChan's own voice.
- The voice is generated just before it is played, so other tasks wait for it for a moment.

## Test

The behavior controller (`include/Behavior.hpp`) does not depend on Arduino or M5Unified, so it is tested on the PC with a C++17 compiler:

```bash
g++ -std=c++17 -Wall -Iinclude test/test_behavior.cpp -o test_behavior
./test_behavior
```

## Troubleshooting

Known problems and their solutions are in [troubleshooting.md](troubleshooting.md):

- The touch screen does not respond (restart the StackChan itself)
- The firmware crashes right after the splash screen
- The upload fails because the serial port is busy

<!-- ## Service Detail
TODO: Update move bluetooth description to bluetooth.md
The BLE service is currently disabled in `src/main.cpp`.

```json
{
  "name": "StackchanService",
  "uuid": "671e0000-8cef-46b7-8af3-2eddeb12803e",
  "characteristics": [
    {
      "name": "timer_chr",
      "uuid": "671e0001-8cef-46b7-8af3-2eddeb12803e",
      "data_type": "UnsignedLong",
      "properties": ["Read", "Notify"]
    },
    {
      "name": "facial_expression_chr",
      "uuid": "671e1000-8cef-46b7-8af3-2eddeb12803e",
      "data_type": "UnsignedChar",
      "properties": ["Read", "Write"]
    },
    {
      "name": "is_servo_activated_chr",
      "uuid": "671e2000-8cef-46b7-8af3-2eddeb12803e",
      "data_type": "Boolean",
      "properties": ["Read", "Write"]
    },
    {
      "name": "servo_pan_angle_chr",
      "uuid": "671e2001-8cef-46b7-8af3-2eddeb12803e",
      "data_type": "UnsignedChar",
      "properties": ["Read", "Write"]
    },
    {
      "name": "servo_tilt_angle_chr",
      "uuid": "671e2002-8cef-46b7-8af3-2eddeb12803e",
      "data_type": "UnsignedChar",
      "properties": ["Read", "Write"]
    }
  ]
}
``` -->

<!-- ## Animation
TODO:  Update description
```mermaid
graph TB

subgraph clips
  clip1
  clip2
end

update_timestamp

``` -->
