# Hardware: communication paths

How the ESP32-S3 of the StackChan (CoreS3 + body) talks to each device. Check this table before adding a peripheral, so that it does not take a pin or an I2C address that is already used.
The GPIO numbers and the I2C addresses are taken from the source of M5GFX / M5Unified / StackChan-BSP and from the [official page](https://docs.m5stack.com/ja/StackChan). Items marked "not checked" were not confirmed.

## SPI

| device | pins | note |
| --- | --- | --- |
| LCD (ILI9342C / E) | MOSI 37, SCLK 36, DC 35, CS 3 | 3-wire SPI: DC shares GPIO35 with MISO (the LCD cannot be read back while it is used) |
| microSD card slot | same bus, CS 4 | CoreS3 side. Not used by this firmware |

The camera (GC0308) uses a parallel interface and its control uses I2C. Its pins are reserved and not checked.

## I2C

Internal bus (`M5.In_I2C`): **SDA 12, SCL 11**, 100 kHz - 400 kHz. All of the following devices are on this bus, so a new device must not use these addresses.

| device | address | note |
| --- | --- | --- |
| touch screen FT6336U | `0x38` | interrupt: touch INT -> AW9523 P1_2 -> AW9523 INTN -> **GPIO21** |
| power management AXP2101 | `0x34` | battery level, charging state, LCD backlight and power rails |
| IO expander AW9523B | `0x58` | collects the interrupt of the touch screen (P1_2) and other signals of the CoreS3. The other uses are not checked |
| audio codec ES7210 (microphones) | `0x40` | |
| audio amplifier AW88298 (speaker) | `0x36` | |
| camera GC0308 (control) | `0x21` | |
| battery monitor INA226 | `0x41` | body |
| NFC ST25R3916 | `0x50` | body. Not used by this firmware |
| head touch sensor Si12T | `0x68` | body. Read through StackChan-BSP |
| IO expander PY32L020 | `0x6F` | body. Servo power (VM_EN) and the 12 RGB LEDs (WS2812C) |
| RTC BM8563, IMU BMI270 / BMM150, proximity / ambient light LTR-553 | not checked | CoreS3 side |

External I2C (`M5.Ex_I2C`, **PORT.A**): SDA 2, SCL 1. This is a separate bus, so a peripheral on PORT.A does not meet the addresses above. This is the first choice for a new I2C peripheral.

## Audio (I2S)

| device | pins | note |
| --- | --- | --- |
| microphones (ES7210) | MCLK 0, BCK 34, WS 33, DATA IN 14 | `I2S_NUM_1` |
| speaker (AW88298) | BCK 34, WS 33, DATA OUT 13 | `I2S_NUM_1` |

The microphone and the speaker **share the I2S port, BCK and WS**. They cannot work at the same time, so [ExpressionVoice](include/ExpressionVoice.hpp) stops the microphone while a voice is played. The codecs are configured through the internal I2C bus.

## UART

| device | pins | note |
| --- | --- | --- |
| servos SCS0009 (pan / tilt) | UART1, TX 6, RX 7, 1 Mbps | started by StackChan-BSP (`servo_init`). The servo power is switched by the PY32 |

## Other ports

| port | pins | note |
| --- | --- | --- |
| PORT.A | G1 (SCL), G2 (SDA) | external I2C |
| PORT.B | G8, G9 | not used by this firmware |
| PORT.C | G17, G18 | not used by this firmware |
| IR transmitter / receiver | not checked | |

Do not use GPIO 0, 3, 4, 6, 7, 11 - 14, 21, 33 - 37 for a new device, because they are in the tables above.
