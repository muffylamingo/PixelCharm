// Every pin of the prototype in one place (pin plan in PLAN.md). Wokwi uses the same pins.
// The keychain board in Faz 4 (XIAO ESP32-S3) will get its own file with the same names.
#pragma once
#include <stdint.h>

// Display (SPI). All on the board's left pin row.
constexpr int PIN_SCK = 12;
constexpr int PIN_MOSI = 11;
constexpr int PIN_CS = 10;         // only Wokwi's display has a CS pin; the real 1.3" screen has none
constexpr int PIN_DC = 9;
constexpr int PIN_RST = 14;
constexpr int PIN_BACKLIGHT = 13;  // only on the real screen; Wokwi's backlight is wired to 3V3

// Buttons: push buttons in Wokwi, bare wire tips you touch on the real board (touch works on GPIO 1-14)
constexpr uint8_t PIN_BUTTON_A = 4;
constexpr uint8_t PIN_BUTTON_B = 5;
