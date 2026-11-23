// Reads one button: filters out contact bounce and tells short presses from long ones.
// The "button" can be a real push button or, on the ESP32-S3, a bare wire tip you touch with a finger.
#pragma once

#include <Arduino.h>

enum class ButtonEvent { None, ShortPress, LongPress };

// Where a button's "pressed" comes from
enum class ButtonInput {
  PullUp,  // a push button (or two wire tips touched together) between the pin and GND
  Touch,   // a finger on the pin's bare wire tip: the chip senses the body's capacitance (GPIO 1-14 on the S3)
};

class Button {
 public:
  explicit Button(uint8_t pin, ButtonInput input = ButtonInput::PullUp) : pin_(pin), input_(input) {}

  void begin() {
    if (input_ == ButtonInput::PullUp) {
      pinMode(pin_, INPUT_PULLUP);
      return;
    }
    // Touch: learn the untouched value first. Don't touch the wires during the first moment after power-on.
    uint32_t settleUntil = millis() + 100;
    while (millis() < settleUntil) touchRead(pin_);  // the first reads start the touch sensor
    float sum = 0;
    for (int i = 0; i < 16; i++) {
      sum += touchRead(pin_);
      delay(5);
    }
    untouched_ = sum / 16;
  }

  // Call on every loop(). Reports a press once, when the button is released,
  // so later tasks can pick an action from how long it was held (1 s, 3 s, 10 s...).
  ButtonEvent update(uint32_t now) {
    bool rawPressed = readPressed();
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

  // Touch only: the latest reading and the level it has to pass to count as a touch
  bool isTouch() const { return input_ == ButtonInput::Touch; }
  uint32_t touchValue() const { return touchValue_; }
  uint32_t touchThreshold() const { return untouched_ * TOUCH_RATIO; }

  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t LONG_PRESS_MS = 1000;
  // On the ESP32-S3 the reading goes UP when a finger touches the pad. A touch must reach this many times the untouched value.
  // Measured on our wires: ~23,500 untouched, 45,000-300,000 with a finger, up to ~37,000 with a hand just nearby.
  static constexpr float TOUCH_RATIO = 1.5f;

 private:
  bool readPressed() {
    if (input_ == ButtonInput::PullUp) return digitalRead(pin_) == LOW;  // pull-up: pressed reads LOW

    touchValue_ = touchRead(pin_);
    bool touched = touchValue_ > touchThreshold();
    // Moving the wires or the room's humidity shifts the untouched value a little: follow it slowly
    if (!touched) untouched_ += (touchValue_ - untouched_) * 0.01f;
    return touched;
  }

  uint8_t pin_;
  ButtonInput input_;
  bool lastRaw_ = false;
  bool pressed_ = false;
  uint32_t lastChangeMs_ = 0;
  uint32_t pressStartMs_ = 0;
  uint32_t lastPressMs_ = 0;
  float untouched_ = 0;
  uint32_t touchValue_ = 0;
};
