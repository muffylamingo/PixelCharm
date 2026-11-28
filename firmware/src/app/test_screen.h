// The test screen (opened from the menu): press counters, hold bars, touch readings and the FPS.
// Started as the button test of task 1.2.
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "../input/button.h"

class TestScreen {
 public:
  // Every press is counted here, whatever screen is on
  void count(char button, ButtonEvent event, uint32_t pressMs) {
    Counts& counts = button == 'A' ? countsA_ : countsB_;
    if (event == ButtonEvent::ShortPress) counts.shortPresses++;
    if (event == ButtonEvent::LongPress) counts.longPresses++;
    lastPressMs_ = pressMs;
  }

  void draw(LGFX_Sprite& canvas, uint32_t now, const Button& a, const Button& b, int brightnessPercent, float fps) const {
    canvas.fillScreen(TFT_BLACK);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(2);
    canvas.drawString("Test", 10, 10);
    char text[28];
    snprintf(text, sizeof(text), "%5.1f FPS", fps);
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString(text, 120, 10);

    snprintf(text, sizeof(text), "A  kisa: %3d", countsA_.shortPresses);
    line(canvas, 40, text);
    snprintf(text, sizeof(text), "   uzun: %3d", countsA_.longPresses);
    line(canvas, 60, text);
    holdBar(canvas, 84, a.heldMs(now));
    snprintf(text, sizeof(text), "B  kisa: %3d", countsB_.shortPresses);
    line(canvas, 104, text);
    snprintf(text, sizeof(text), "   uzun: %3d", countsB_.longPresses);
    line(canvas, 124, text);
    holdBar(canvas, 148, b.heldMs(now));
    snprintf(text, sizeof(text), "Son: %5lu ms", (unsigned long)lastPressMs_);
    line(canvas, 168, text);
    snprintf(text, sizeof(text), "Parlaklik: %3d%%", brightnessPercent);
    line(canvas, 190, text);

    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setTextSize(1);
    if (a.isTouch()) {
      // Touch readings next to the level they must pass: handy when a touch doesn't register
      snprintf(text, sizeof(text), "A %lu/%lu", (unsigned long)a.touchValue(), (unsigned long)a.touchThreshold());
      canvas.drawString(text, 10, 210);
      snprintf(text, sizeof(text), "B %lu/%lu", (unsigned long)b.touchValue(), (unsigned long)b.touchThreshold());
      canvas.drawString(text, 125, 210);
    }
    canvas.drawString("A kisa=geri  A uzun=menu", 10, 226);
  }

 private:
  struct Counts {
    int shortPresses = 0;
    int longPresses = 0;
  };

  static void line(LGFX_Sprite& canvas, int y, const char* text) {
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(2);
    canvas.drawString(text, 10, y);
  }

  // Fills up while the button is held; turns green once it counts as a long press
  static void holdBar(LGFX_Sprite& canvas, int y, uint32_t heldMs) {
    constexpr int X = 10, WIDTH = 220, HEIGHT = 6;
    int filled = WIDTH * min(heldMs, Button::LONG_PRESS_MS) / Button::LONG_PRESS_MS;
    uint16_t color = heldMs >= Button::LONG_PRESS_MS ? TFT_GREEN : TFT_YELLOW;
    canvas.fillRect(X, y, filled, HEIGHT, color);
    canvas.fillRect(X + filled, y, WIDTH - filled, HEIGHT, TFT_DARKGREY);
  }

  Counts countsA_, countsB_;
  uint32_t lastPressMs_ = 0;
};
