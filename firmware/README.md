# Firmware

ESP32-S3 firmware for PixelCharm: C++ with the Arduino framework on PlatformIO, drawing with [LovyanGFX](https://github.com/lovyan03/LovyanGFX).

## Build and upload

Open this folder in VS Code with the PlatformIO extension.

| Environment | For |
|---|---|
| `devkit` (default) | The real prototype: ESP32-S3-DevKitC-1 N16R8 + 1.3" ST7789, touch buttons |
| `wokwi` | The Wokwi simulator: same pins, a 2.8" ILI9341 display and push buttons |

- **Upload** puts the program on the board. Use the board's **COM/UART** USB port.
- **Upload Filesystem Image** (PlatformIO → *Platform*) copies `data/` onto the board's LittleFS disk. Make `data/` with [`tools/prepare_images.py`](../tools/prepare_images.py). Uploading one doesn't erase the other.

The flash layout is in [`partitions_8mb.csv`](partitions_8mb.csv): two 2 MB app slots (room for over-the-air updates later) and a 3.9 MB LittleFS disk, sized for 8 MB so it also fits the smaller board planned for the keychain.

## How the code is organised

```
src/
├── main.cpp           setup() and loop(): just runs the App
├── board_devkit.h     every pin in one place
├── app/               app.h: the state machine (BOOT -> THEME <-> MENU -> TEST)
│                      menu.h, settings.h (NVS), test_screen.h
├── themes/            theme.h: the interface every theme implements
│                      face.h, dice.h, picture.h (JPEG + GIF), catalog.h (reads /themes/index.json)
├── input/             button.h: debounce, short and long presses, push or touch
└── display/           lgfx_config.h: which display and how it is wired
```

Every frame is drawn into a 240×240 sprite in RAM and sent to the display in one go, so nothing flickers. Each theme implements `enter`, `frame`, `action`, `leave` and `drawIcon`; the app never needs to know which kind of theme is on screen.

## Libraries

All pinned in [`platformio.ini`](platformio.ini): LovyanGFX 1.2.32, [JPEGDEC](https://github.com/bitbank2/JPEGDEC) 1.8.4, [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) 2.2.3, [ArduinoJson](https://arduinojson.org/) 7.4.3, on `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.12).

## Serial commands

At 115200 baud you can drive the board from the computer: `a`/`b` tap A/B, `A` holds A, `t` runs the d20 fairness test, `s` sends a screenshot, `m` prints free memory. The full list is in [`tools/README.md`](../tools/README.md).
