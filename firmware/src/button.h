// Reads one push button: filters out contact bounce and tells short presses from long ones.
#pragma once

#include <Arduino.h>

enum class ButtonEvent { None, ShortPress, LongPress };

class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}

  void begin() { pinMode(pin_, INPUT_PULLUP); }

  // Call on every loop(). Reports a press once, when the button is released,
  // so later tasks can pick an action from how long it was held (1 s, 3 s, 10 s...).
  ButtonEvent update(uint32_t now) {
    bool rawPressed = digitalRead(pin_) == LOW;  // pull-up: pressed reads LOW
    if (rawPressed != lastRaw_) {
      // The contacts moved (a real press or a bounce): start waiting for them to settle again
      lastRaw_ = rawPressed;
      lastChangeMs_ = now;
    }
    if (now - lastChangeMs_ < DEBOUNCE_MS || rawPressed == pressed_) return ButtonEvent::None;

    // The reading has been steady for DEBOUNCE_MS and differs from what we had: accept it
    pressed_ = rawPressed;
    if (pressed_) {
      pressStartMs_ = now;
      return ButtonEvent::None;
    }
    lastPressMs_ = now - pressStartMs_;
    return lastPressMs_ >= LONG_PRESS_MS ? ButtonEvent::LongPress : ButtonEvent::ShortPress;
  }

  // How long the button has been held so far (0 when it is not pressed)
  uint32_t heldMs(uint32_t now) const { return pressed_ ? now - pressStartMs_ : 0; }

  // How long the last finished press lasted
  uint32_t lastPressMs() const { return lastPressMs_; }

  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t LONG_PRESS_MS = 1000;

 private:
  uint8_t pin_;
  bool lastRaw_ = false;
  bool pressed_ = false;
  uint32_t lastChangeMs_ = 0;
  uint32_t pressStartMs_ = 0;
  uint32_t lastPressMs_ = 0;
};
