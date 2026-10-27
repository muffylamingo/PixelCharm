// The basic face theme: two eyes that blink and look around, and a happy face when you press B.
#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class Face {
 public:
  // Call once per frame before draw(). Works out where the eyes are and how open they are.
  void update(uint32_t now, float seconds) {
    updateBlink(now);
    updateLook(now, seconds);

    uint32_t sinceReact = now - reactStartMs_;
    if (sinceReact >= HAPPY_MS) happy_ = false;
    // A small hop at the start of the happy face: up and back down along half a sine wave
    hopY_ = happy_ && sinceReact < HOP_MS ? sinf(PI * sinceReact / HOP_MS) * HOP_HEIGHT : 0;
  }

  // B short press: be happy for a moment
  void react(uint32_t now) {
    happy_ = true;
    reactStartMs_ = now;
    targetX_ = targetY_ = 0;  // look straight at you...
    lookStartMs_ = now;
    lookGapMs_ = HAPPY_MS + 500;  // ...and keep looking for a little while
  }

  // Draws the whole face, background included, so the caller never has to erase anything
  void draw(LGFX_Sprite& canvas) const {
    canvas.fillScreen(BACKGROUND);
    int centerX = 120 + lookX_;
    int eyeY = 100 + lookY_ - hopY_;
    // The mouth moves half as far as the eyes, which makes the face look a bit round instead of flat
    int mouthX = 120 + lookX_ / 2;
    int mouthY = 162 + lookY_ / 2 - hopY_;

    for (int side : {-1, 1}) {
      int eyeX = centerX + side * EYE_GAP;
      if (happy_) {
        canvas.fillArc(eyeX, eyeY + 12, 20, 28, 200, 340, EYE_COLOR);  // "^": the top part of a thick ring
        canvas.fillEllipse(eyeX + side * 6, eyeY + 34, 16, 8, BLUSH_COLOR);
      } else {
        drawOpenEye(canvas, eyeX, eyeY);
      }
    }

    if (happy_) {
      canvas.fillArc(mouthX, mouthY - 6, 0, 26, 0, 180, EYE_COLOR);  // big open smile: the bottom half of a circle
    } else {
      canvas.fillArc(mouthX, mouthY - 10, 12, 16, 30, 150, EYE_COLOR);  // small smile
    }
  }

 private:
  void updateBlink(uint32_t now) {
    uint32_t sinceBlink = now - blinkStartMs_;
    if (sinceBlink >= BLINK_MS + blinkGapMs_) {
      blinkStartMs_ = now;
      sinceBlink = 0;
      // Sometimes blink twice in a row, like people do
      blinkGapMs_ = random(100) < 20 ? 120 : random(2000, 6000);
    }
    // 1 = open, 0 = closed. Goes 1 -> 0 -> 1 during the blink
    openness_ = sinceBlink < BLINK_MS ? fabsf(1 - 2.0f * sinceBlink / BLINK_MS) : 1;
  }

  void updateLook(uint32_t now, float seconds) {
    if (now - lookStartMs_ >= lookGapMs_) {
      lookStartMs_ = now;
      lookGapMs_ = random(1200, 3500);
      const auto& spot = LOOK_SPOTS[random(sizeof(LOOK_SPOTS) / sizeof(LOOK_SPOTS[0]))];
      targetX_ = spot[0];
      targetY_ = spot[1];
    }
    // Cover part of the remaining distance each frame: fast at first, then slowing down.
    // Multiplying by the frame time keeps the speed the same at any FPS.
    float step = min(1.0f, seconds * LOOK_SPEED);
    lookX_ += (targetX_ - lookX_) * step;
    lookY_ += (targetY_ - lookY_) * step;
  }

  void drawOpenEye(LGFX_Sprite& canvas, int eyeX, int eyeY) const {
    int height = max(6, int(EYE_HEIGHT * openness_));
    int radius = min(EYE_RADIUS, height / 2);
    canvas.fillSmoothRoundRect(eyeX - EYE_WIDTH / 2, eyeY - height / 2, EYE_WIDTH, height, radius, EYE_COLOR);
    // A white shine in the corner makes the eye look glossy. Hidden while the eye is nearly shut.
    if (openness_ > 0.6f) canvas.fillSmoothCircle(eyeX - 10, eyeY - height / 2 + 16, 7, TFT_WHITE);
  }

  static constexpr uint16_t BACKGROUND = TFT_BLACK;
  static constexpr uint16_t EYE_COLOR = lgfx::color565(90, 220, 255);    // soft cyan
  static constexpr uint16_t BLUSH_COLOR = lgfx::color565(255, 120, 160);  // pink cheeks

  static constexpr int EYE_GAP = 50;  // from the middle of the face to the middle of each eye
  static constexpr int EYE_WIDTH = 56;
  static constexpr int EYE_HEIGHT = 72;
  static constexpr int EYE_RADIUS = 18;

  static constexpr uint32_t BLINK_MS = 150;
  static constexpr uint32_t HAPPY_MS = 1500;
  static constexpr uint32_t HOP_MS = 300;
  static constexpr float HOP_HEIGHT = 10;  // pixels
  static constexpr float LOOK_SPEED = 10;

  // Where the eyes can look, as an offset from the middle in pixels. The middle is listed twice
  // so the face looks straight ahead more often than anywhere else.
  static constexpr int8_t LOOK_SPOTS[][2] = {{0, 0}, {0, 0}, {-28, 0}, {28, 0}, {-22, -14}, {22, -14}, {-20, 12}, {20, 12}};

  float openness_ = 1;
  uint32_t blinkStartMs_ = 0;
  uint32_t blinkGapMs_ = 3000;

  float lookX_ = 0, lookY_ = 0;
  float targetX_ = 0, targetY_ = 0;
  uint32_t lookStartMs_ = 0;
  uint32_t lookGapMs_ = 2000;

  bool happy_ = false;
  uint32_t reactStartMs_ = 0;
  float hopY_ = 0;
};
