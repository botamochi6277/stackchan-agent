# Troubleshooting

## The touch screen does not respond (flick, tap)

Symptom: the head touch sensor and the microphone work, but the touch screen does not react at all. Flicking the screen does not change the expression.

What was checked:

- The touch screen (FT6336U, I2C `0x38`) and the other devices do not share a bus line. The LCD uses SPI (MOSI 37, SCLK 36, DC 35, CS 3), the touch screen uses the internal I2C bus (SDA 12, SCL 11) with its interrupt on GPIO21 (through the AW9523), and the servos use UART1 (GPIO 6 / 7). See [hardware.md](hardware.md). No I2C address is used twice.
- Reading the touch controller directly returned "no touch point" even when the interrupt (GPIO21) went low, and `M5.Touch.getCount()` stayed 0. `M5.Touch.isEnabled()` was true.
- The state did not change by flashing another firmware, and the same symptom appeared with the plain demo of stackchan-voice.

Solution: **restart the StackChan itself** (unplug the USB cable and plug it again, or turn the power off and on). The touch screen worked again after the restart, and no code was changed for it. Flashing a firmware does not reset the touch controller, so it may stay in the abnormal state across flashing.

If it happens again, check the touch screen with the demo of stackchan-display or stackchan-voice first. If the demo does not respond either, the cause is not this firmware.

## The firmware crashes right after the splash screen

Symptom: after the "CHANt" splash, the firmware stops with `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)`. `EXCVADDR` is a small address such as `0x50`, and the backtrace goes through `Display::update()` -> `pushSprite`.

Cause: `Display` keeps the address of `M5.Lcd` when it is constructed. A global `Display` may be constructed before `M5` is, depending on the order of the static initialization, and then the canvas has no valid destination.

Solution: `Display` is constructed in `setup()` after `M5StackChan.begin()` (see `display_storage` in `src/main.cpp`). Do not make it a global object again.

To read a backtrace, use the `addr2line` of the toolchain:

```bash
xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/<env>/firmware.elf <addresses>
```

## The upload fails with "Could not open COM3, the port is busy"

Another program is holding the serial port, such as a running `pio device monitor` or a serial monitor of an IDE. Close it and upload again.
