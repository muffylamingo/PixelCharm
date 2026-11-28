# Wiring the prototype

The ESP32-S3-DevKitC-1 (N16R8) and the 1.3" ST7789 screen are joined with **7 female-to-female jumper wires**. No breadboard: this board is about 2.8 cm wide between its pin rows, the full width of a small breadboard, so there are no free holes left next to it.

Every wire goes to the board's **left pin row** (the one that starts with 3V3), so you can lay the board upside down and reach all of them.

**Unplug the USB cable while you wire.** Check every wire once more before you plug it back in.

## Screen

| Screen pin | Board pin | What it is |
|---|---|---|
| GND | G (GND) | Ground |
| VCC | **3V3** | Power. Never 5V: the screen and the ESP32 run on 3.3 V |
| SCK | GPIO 12 | SPI clock |
| SDA | GPIO 11 | SPI data (MOSI). "SDA" is only the label: this is SPI, not I²C |
| RES | GPIO 14 | Reset |
| DC | GPIO 9 | Data / command |
| BLK | GPIO 13 | Backlight, dimmed with PWM |

The screen has **no CS pin**. It is the only device on the SPI bus, so the firmware sets `pin_cs = -1` and uses **SPI mode 3**, the only mode these CS-less ST7789 modules listen in. Its IPS panel also needs `invert = true`. All of this is in [`firmware/src/display/lgfx_config.h`](../firmware/src/display/lgfx_config.h); every pin number is in [`firmware/src/board_devkit.h`](../firmware/src/board_devkit.h).

## Buttons (touch)

| Wire | Board pin |
|---|---|
| A | GPIO 4 |
| B | GPIO 5 |

One male-to-female jumper wire per button: the female end on the board pin, the bare male tip free. **Touching the tip with a finger is a press.** GPIO 1–14 are capacitive touch pins on the ESP32-S3, so no push button or ground wire is needed. Leave the wires alone for the first second after power-on: that is when the firmware learns their untouched value.

To use real push buttons instead, wire each one between its pin and GND and remove `-DBUTTONS_TOUCH` from [`firmware/platformio.ini`](../firmware/platformio.ini).

## USB

The board has two USB-C ports. Uploads and the Serial Monitor use the one marked **COM** (or UART), through the CH343 USB-serial chip. The other one (USB/OTG) is the ESP32-S3's own USB and won't take uploads with this setup.

## Pins to avoid

GPIO 0, 3, 45 and 46 change how the board starts up · 19 and 20 are the USB port · 26–37 belong to the flash and the PSRAM · 43 and 44 are the serial port.
