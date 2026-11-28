// The device's "brain": which state it is in, what each touch does there, and the main loop.
//
//   BOOT ──▶ THEME ◀──▶ MENU ──▶ TEST
//              ▲                   │
//              └───────────────────┘
//
//   THEME: A short = next theme, A long = menu, B short = the theme's action (roll the die, smile...)
//   MENU:  A short = next square, A long = back to the theme, B short = open the square
//   TEST:  A short = back to the theme, A long = menu
#pragma once
#include <Arduino.h>

#include "../board_devkit.h"
#include "../display/lgfx_config.h"
#include "../input/button.h"
#include "../themes/catalog.h"
#include "menu.h"
#include "settings.h"
#include "test_screen.h"

class App {
 public:
  void begin() {
    Serial.begin(115200);
    // Which board are we on? The real N16R8 board should say 16 MB flash and 8 MB PSRAM
    Serial.printf("Kart: %s %lu MHz, flash %lu MB, PSRAM %lu MB\n", ESP.getChipModel(), (unsigned long)getCpuFrequencyMhz(),
                  (unsigned long)(ESP.getFlashChipSize() >> 20), (unsigned long)(ESP.getPsramSize() >> 20));
    settings_.begin();
    brightness_ = settings_.brightnessLevel() % BRIGHTNESS_STEPS;

    display_.init();
    display_.fillScreen(TFT_BLACK);
    applyBrightness();
    shadeWokwiRows();
    // An invisible 240x240 drawing board in RAM (240 x 240 x 2 bytes = 115,200 bytes). Each frame is drawn here
    // first and then sent to the display in one go, so the display never shows a half-drawn frame.
    canvas_.setColorDepth(16);
    if (!canvas_.createSprite(SCREEN_SIZE, SCREEN_SIZE)) {
      Serial.println("HATA: sprite icin yeterli RAM yok");
      display_.drawString("Sprite icin RAM yetmedi", 8, 8);
      return;
    }
    Serial.printf("Sprite hazir: %lu bayt. Kalan RAM: %lu bayt\n", (unsigned long)canvas_.bufferLength(),
                  (unsigned long)ESP.getFreeHeap());

    // BOOT: the splash stays up while the buttons learn their untouched value and the themes load
    bootStartMs_ = millis();
    drawSplash();
    canvas_.pushSprite(0, 0);
    buttonA_.begin();
    buttonB_.begin();
    if (buttonA_.isTouch()) {
      Serial.printf("Butonlar dokunmatik: A esigi %lu, B esigi %lu\n", (unsigned long)buttonA_.touchThreshold(),
                    (unsigned long)buttonB_.touchThreshold());
    }
    themes_.load();
    current_ = themes_.find(settings_.themeId());
    Serial.println("Merhaba PixelCharm! Serial komutlari: a/b = A/B kisa, A = A uzun (menu), t = zar testi,");
    Serial.println("  s = ekran goruntusu, +/- = sonraki atis 20/1, d = dokunma degerleri, m = bellek");
    lastFrameMs_ = fpsWindowStartMs_ = millis();
  }

  void loop() {
    uint32_t now = millis();
    float seconds = (now - lastFrameMs_) / 1000.0f;  // time since the previous frame
    lastFrameMs_ = now;

    handleButtons(now);
    handleSerialCommands(now);

    // Draw the current state into the canvas...
    switch (state_) {
      case State::Boot:
        if (now - bootStartMs_ >= BOOT_MS) showTheme(current_, now);  // the last theme from NVS
        break;
      case State::Theme:
        themes_[current_].frame(canvas_, now, seconds);
        break;
      case State::Menu:
        if (menu_.timedOut(now)) {
          showTheme(current_, now);
        } else {
          menu_.draw(canvas_, BRIGHTNESS_LEVELS[brightness_]);
        }
        break;
      case State::Test:
        test_.draw(canvas_, now, buttonA_, buttonB_, BRIGHTNESS_LEVELS[brightness_], fps_);
        break;
    }
    // ...then send the finished frame to the display in one go
    canvas_.pushSprite(0, 0);
    updateFps(now);
  }

 private:
  enum class State { Boot, Theme, Menu, Test };

  // ---------- Moving between states ----------

  void showTheme(size_t index, uint32_t now) {
    if (state_ == State::Theme) themes_[current_].leave();
    current_ = index;
    state_ = State::Theme;
    Serial.printf("Tema %u/%u: %s\n", (unsigned)(current_ + 1), (unsigned)themes_.count(), themes_[current_].name().c_str());
    themes_[current_].enter(canvas_, now);
    settings_.setThemeId(themes_[current_].id());  // so the device starts with this theme next time
  }

  void openMenu(uint32_t now) {
    if (state_ == State::Theme) themes_[current_].leave();  // the menu takes over the canvas
    state_ = State::Menu;
    menu_.open(themes_, current_, now);
  }

  // B in the menu: open whatever square is chosen
  void chooseMenuItem(uint32_t now) {
    switch (menu_.selectedItem()) {
      case Menu::Item::Theme:
        showTheme(menu_.selectedTheme(), now);
        break;
      case Menu::Item::Brightness:  // stay in the menu, so you can press B again until it looks right
        brightness_ = (brightness_ + 1) % BRIGHTNESS_STEPS;
        applyBrightness();
        settings_.setBrightnessLevel(brightness_);
        menu_.refresh(now);
        break;
      case Menu::Item::Test:
        state_ = State::Test;
        break;
    }
  }

  // ---------- Buttons ----------

  void handleButtons(uint32_t now) {
    ButtonEvent a = buttonA_.update(now);
    ButtonEvent b = buttonB_.update(now);
    if (a != ButtonEvent::None) pressed('A', a, buttonA_.lastPressMs(), now);
    if (b != ButtonEvent::None) pressed('B', b, buttonB_.lastPressMs(), now);
  }

  void pressed(char button, ButtonEvent event, uint32_t pressMs, uint32_t now) {
    bool isLong = event == ButtonEvent::LongPress;
    Serial.printf("%c %s basis (%lu ms)\n", button, isLong ? "uzun" : "kisa", (unsigned long)pressMs);
    test_.count(button, event, pressMs);
    if (button == 'A') {
      pressA(isLong, now);
    } else if (!isLong) {
      pressB(now);  // B's long presses get their jobs in Faz 2 and 4 (see the button map)
    }
  }

  void pressA(bool isLong, uint32_t now) {
    switch (state_) {
      case State::Theme:
        if (isLong) {
          openMenu(now);
        } else {
          showTheme((current_ + 1) % themes_.count(), now);
        }
        break;
      case State::Menu:
        if (isLong) {
          showTheme(current_, now);
        } else {
          menu_.next(now);
        }
        break;
      case State::Test:
        if (isLong) {
          openMenu(now);
        } else {
          showTheme(current_, now);
        }
        break;
      case State::Boot:
        break;  // still starting up
    }
  }

  void pressB(uint32_t now) {
    if (state_ == State::Theme) themes_[current_].action(now);
    if (state_ == State::Menu) chooseMenuItem(now);
  }

  // ---------- Serial commands: test from the computer without touching the wires ----------

  // Type one of these letters in the Serial Monitor and press Enter (or let a tools/ script send it)
  void handleSerialCommands(uint32_t now) {
    while (Serial.available() > 0) {
      switch (Serial.read()) {
        case 'a':
          pressed('A', ButtonEvent::ShortPress, 0, now);
          break;
        case 'A':
          pressed('A', ButtonEvent::LongPress, 0, now);
          break;
        case 'b':
          pressed('B', ButtonEvent::ShortPress, 0, now);
          break;
        case 't':
          printFairnessTest(Serial, 10000);
          break;
        case 's':
          sendScreenshot();
          break;
        case '+':
          themes_.dice().forceNext(20);
          Serial.println("Test: sonraki atis 20");
          break;
        case '-':
          themes_.dice().forceNext(1);
          Serial.println("Test: sonraki atis 1");
          break;
        case 'm':  // memory and uptime: a long test shows whether memory slowly runs out (a leak)
          Serial.printf("MEM heap %u KB (en az %u KB), PSRAM %u KB, %lu sn\n", (unsigned)(ESP.getFreeHeap() / 1024),
                        (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024),
                        (unsigned long)(millis() / 1000));
          break;
        case 'd':  // touch readings, to tune the touch buttons
          Serial.printf("TOUCH A %lu %lu B %lu %lu\n", (unsigned long)buttonA_.touchValue(),
                        (unsigned long)buttonA_.touchThreshold(), (unsigned long)buttonB_.touchValue(),
                        (unsigned long)buttonB_.touchThreshold());
          break;
      }
    }
  }

  // Sends the last frame to the computer as raw pixels (2 bytes each). tools/screenshot.py turns it into a PNG.
  // At 115200 baud the 115 KB take about 10 seconds; the screen stands still meanwhile.
  void sendScreenshot() {
    Serial.printf("SCREEN %d %d\n", (int)canvas_.width(), (int)canvas_.height());
    Serial.write(static_cast<const uint8_t*>(canvas_.getBuffer()), canvas_.bufferLength());
    Serial.println();
  }

  // ---------- Display helpers ----------

  void drawSplash() {
    canvas_.fillScreen(TFT_BLACK);
    splashFace_.drawIcon(canvas_, 70, 30, 100);  // the face greets you
    canvas_.setFont(&fonts::FreeSansBold12pt7b);
    canvas_.setTextDatum(textdatum_t::middle_center);
    canvas_.setTextColor(TFT_WHITE);
    canvas_.drawString("PixelCharm", 120, 156);
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextColor(TFT_DARKGREY);
    canvas_.drawString("v0.1 - Faz 1", 120, 182);
    if (buttonA_.isTouch()) {
      // The buttons are learning their untouched value right now
      canvas_.setTextColor(TFT_LIGHTGREY);
      canvas_.drawString("Kablolara dokunma...", 120, 214);
    }
    canvas_.setFont(&fonts::Font0);
    canvas_.setTextDatum(textdatum_t::top_left);
  }

  // Only Wokwi's 240x320 display has rows below our 240x240 area: shade them
  void shadeWokwiRows() {
    if (display_.height() <= SCREEN_SIZE) return;
    display_.fillRect(0, SCREEN_SIZE, display_.width(), display_.height() - SCREEN_SIZE, TFT_DARKGREY);
    display_.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    display_.drawString("Wokwi only: outside the 240x240 area", 8, SCREEN_SIZE + 8);
  }

  void applyBrightness() {
    display_.setBrightness(BRIGHTNESS_LEVELS[brightness_] * 255 / 100);  // LovyanGFX takes 0-255
  }

  // Counts the frames sent to the display and works out the FPS once a second
  void updateFps(uint32_t now) {
    framesInWindow_++;
    if (now - fpsWindowStartMs_ < 1000) return;
    fps_ = framesInWindow_ * 1000.0f / (now - fpsWindowStartMs_);
    framesInWindow_ = 0;
    fpsWindowStartMs_ = now;
  }

  // Our real screen is 240x240. Wokwi's ILI9341 is 240x320, so we only use the top 240 rows.
  static constexpr int SCREEN_SIZE = 240;
  static constexpr uint32_t BOOT_MS = 1200;  // how long the splash stays up at least
  // Backlight levels in percent. Wokwi's backlight is wired to 3V3, so there it is only a number.
  static constexpr int BRIGHTNESS_LEVELS[] = {25, 50, 75, 100};
  static constexpr int BRIGHTNESS_STEPS = 4;

#ifdef BUTTONS_TOUCH
  // Real board for now: no push buttons yet, so you touch the bare tip of the wire on GPIO 4 or 5 with a finger
  static constexpr ButtonInput BUTTON_INPUT = ButtonInput::Touch;
#else
  static constexpr ButtonInput BUTTON_INPUT = ButtonInput::PullUp;  // Wokwi's push buttons
#endif

  LGFX display_;
  LGFX_Sprite canvas_{&display_};
  Button buttonA_{PIN_BUTTON_A, BUTTON_INPUT};
  Button buttonB_{PIN_BUTTON_B, BUTTON_INPUT};
  Settings settings_;
  ThemeCatalog themes_;
  Face splashFace_;  // only for the splash: the catalog's themes load after it is up
  Menu menu_;
  TestScreen test_;

  State state_ = State::Boot;
  size_t current_ = 0;  // the theme on screen (or the one to go back to from the menu)
  int brightness_ = 3;
  uint32_t bootStartMs_ = 0;
  uint32_t lastFrameMs_ = 0;
  uint32_t fpsWindowStartMs_ = 0;
  uint32_t framesInWindow_ = 0;
  float fps_ = 0;
};
