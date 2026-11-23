// The d20 theme: a twenty-sided die you roll with B. A natural 20 turns gold, a natural 1 turns red.
#pragma once
#include <Arduino.h>
#include <esp_random.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// A fair roll from 1 to `sides`, using the chip's hardware random number generator.
// esp_random() gives one of 2^32 numbers. 2^32 is not a multiple of 20, so "esp_random() % 20" would make
// 1-16 a tiny bit more likely than 17-20 (modulo bias). Instead we throw away the few numbers at the very top
// that cause it and draw again, so every result has exactly the same number of ways to come up.
// (Arduino's random(20) is just "esp_random() % 20", which is why we don't use it here.)
inline int rollDie(uint32_t sides) {
  const uint32_t limit = UINT32_MAX - UINT32_MAX % sides;  // the biggest multiple of `sides` we can use
  uint32_t value;
  do {
    value = esp_random();
  } while (value >= limit);  // for a d20 this happens about 4 times in a billion
  return value % sides + 1;
}

// Rolls a d20 `rolls` times and prints how often each number came up.
// tools/dice_fairness.py reads these lines and checks them with a chi-square test.
inline void printFairnessTest(Print& out, uint32_t rolls) {
  uint32_t counts[21] = {};
  uint32_t startUs = micros();
  for (uint32_t i = 0; i < rolls; i++) counts[rollDie(20)]++;
  uint32_t elapsedUs = micros() - startUs;

  out.printf("D20_TEST_START %lu\n", (unsigned long)rolls);
  float expected = rolls / 20.0f;  // a fair die gives each number 1/20 of the rolls
  float chiSquare = 0;
  for (int face = 1; face <= 20; face++) {
    out.printf("%d %lu\n", face, (unsigned long)counts[face]);
    float diff = counts[face] - expected;
    chiSquare += diff * diff / expected;
  }
  out.printf("D20_TEST_END\n");
  out.printf("%lu atis %lu mikrosaniyede. Ki-kare: %.2f (30.14'ten kucukse zar adil gorunuyor)\n", (unsigned long)rolls,
             (unsigned long)elapsedUs, chiSquare);
}

class Dice {
 public:
  // B short press: roll. Presses during a roll are ignored, so one roll can't start on top of another.
  void roll(uint32_t now) {
    if (phase_ == Phase::Rolling) return;
    // The result is picked right away. The animation only builds up the suspense.
    result_ = forcedNext_ != 0 ? forcedNext_ : rollDie(SIDES);
    forcedNext_ = 0;
    phase_ = Phase::Rolling;
    rollStartMs_ = lastFlipMs_ = now;
    flipGapMs_ = 0;
  }

  // Testing only (a Serial command): the next roll gives this number, to check the nat 20 and nat 1 animations
  void forceNext(int value) { forcedNext_ = value; }

  // Call once per frame before draw()
  void update(uint32_t now) {
    if (phase_ == Phase::Rolling) updateRoll(now);

    uint32_t sinceLanding = now - landedMs_;
    bool landed = phase_ == Phase::Landed;
    // The result pops out: starts at 0.8x, swells to ~1.2x and settles at 1x (a sine bump on a straight line)
    if (landed && sinceLanding < POP_MS) {
      float t = float(sinceLanding) / POP_MS;
      numberScale_ = 0.8f + 0.2f * t + 0.3f * sinf(PI * t);
    } else {
      numberScale_ = phase_ == Phase::Rolling ? 0.8f : 1;
    }
    // A natural 1 shakes its head: side to side, smaller and smaller
    if (isNat1() && sinceLanding < SHAKE_MS) {
      float t = float(sinceLanding) / SHAKE_MS;
      shakeX_ = sinf(t * 6 * PI) * SHAKE_PIXELS * (1 - t);
    } else {
      shakeX_ = 0;
    }
    // A natural 20 throws sparkles: 0 -> 1 while they fly, -1 when there are none
    sparkleT_ = isNat20() && sinceLanding < SPARKLE_MS ? float(sinceLanding) / SPARKLE_MS : -1;
  }

  // Draws the whole screen, background included
  void draw(LGFX_Sprite& canvas) const {
    canvas.fillScreen(TFT_BLACK);
    const Rgb& color = dieColor();
    float centerX = CENTER_X + shakeX_;

    // Corner points of the die. 0-5: the outline (a hexagon), 6-8: the front face (a triangle).
    // All of them turn together by spin_ degrees while the die rolls.
    float x[9], y[9];
    for (int i = 0; i < 6; i++) placePoint(centerX, -90 + 60 * i, OUTER_RADIUS, x[i], y[i]);
    for (int i = 0; i < 3; i++) placePoint(centerX, -90 + 120 * i, INNER_RADIUS, x[6 + i], y[6 + i]);

    // Faces: the ones turned towards the light (top left) are brighter. The front face is the brightest.
    for (const auto& face : FACES) {
      float brightness = 1;
      if (&face != &FACES[0]) {
        float dx = (x[face[0]] + x[face[1]] + x[face[2]]) / 3 - centerX;
        float dy = (y[face[0]] + y[face[1]] + y[face[2]]) / 3 - CENTER_Y;
        float towardsLight = (dx * LIGHT_X + dy * LIGHT_Y) / sqrtf(dx * dx + dy * dy);  // -1 (away) .. 1 (facing it)
        brightness = 0.55f + 0.3f * towardsLight;
      }
      canvas.fillTriangle(x[face[0]], y[face[0]], x[face[1]], y[face[1]], x[face[2]], y[face[2]], shade(color, brightness));
    }
    // Edges on top: smooth, slightly thick lines in a lighter tint of the die colour
    uint16_t edgeColor = lgfx::color565((color.r + 255) / 2, (color.g + 255) / 2, (color.b + 255) / 2);
    for (const auto& edge : EDGES) canvas.drawWideLine(x[edge[0]], y[edge[0]], x[edge[1]], y[edge[1]], 1.2f, edgeColor);

    drawNumber(canvas, centerX);
    if (sparkleT_ >= 0) drawSparkles(canvas);
    drawLabels(canvas);

    // Leave the canvas the way the other screens expect it (default font, text from the top left)
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextDatum(textdatum_t::top_left);
  }

 private:
  enum class Phase { Waiting, Rolling, Landed };
  struct Rgb {
    uint8_t r, g, b;
  };

  void updateRoll(uint32_t now) {
    uint32_t elapsed = now - rollStartMs_;
    if (elapsed >= ROLL_MS) {
      phase_ = Phase::Landed;
      landedMs_ = now;
      shown_ = result_;
      spin_ = 0;
      if (result_ == SIDES) pickSparkles();
      return;
    }
    float t = float(elapsed) / ROLL_MS;  // 0 -> 1 during the roll
    // Two full turns, fast at first and slowing down (ease-out). 720 degrees later it is upright again.
    float slowDown = 1 - t;
    spin_ = 720 * (1 - slowDown * slowDown * slowDown);
    // The numbers flip fast at first (every 40 ms) and slower near the end (every ~240 ms)
    if (now - lastFlipMs_ >= flipGapMs_) {
      int next;
      do {
        next = rollDie(SIDES);
      } while (next == shown_);  // always show a different number, so every flip is visible
      shown_ = next;
      lastFlipMs_ = now;
      flipGapMs_ = 40 + uint32_t(200 * t * t);
    }
  }

  // "Natural" = the number the die itself shows, before any bonuses are added (D&D)
  bool isNat20() const { return phase_ == Phase::Landed && result_ == SIDES; }
  bool isNat1() const { return phase_ == Phase::Landed && result_ == 1; }

  const Rgb& dieColor() const {
    if (isNat20()) return GOLD;
    if (isNat1()) return RED;
    return PURPLE;
  }

  static uint16_t shade(const Rgb& color, float brightness) {
    return lgfx::color565(uint8_t(color.r * brightness), uint8_t(color.g * brightness), uint8_t(color.b * brightness));
  }

  // A point `radius` pixels away from the die's centre, in the direction `degrees` (0 = right, 90 = down)
  void placePoint(float centerX, float degrees, float radius, float& px, float& py) const {
    float angle = (degrees + spin_) * DEG_TO_RAD;
    px = centerX + cosf(angle) * radius;
    py = CENTER_Y + sinf(angle) * radius;
  }

  void drawNumber(LGFX_Sprite& canvas, float centerX) const {
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(numberScale_);
    // Dark digits on the light gold face, white everywhere else
    uint16_t textColor = isNat20() ? lgfx::color565(70, 40, 0) : TFT_WHITE;
    canvas.setTextColor(textColor);
    if (shown_ == 0) {
      canvas.drawString("?", centerX, CENTER_Y + NUMBER_NUDGE_Y);  // nothing rolled yet
    } else {
      canvas.drawNumber(shown_, centerX, CENTER_Y + NUMBER_NUDGE_Y);
    }
  }

  void drawLabels(LGFX_Sprite& canvas) const {
    canvas.setTextSize(1);
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(textdatum_t::top_left);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("d20", 8, 6);

    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(textdatum_t::middle_center);
    if (isNat20()) {
      canvas.setTextColor(shade(GOLD, 1));
      canvas.drawString("NAT 20!", CENTER_X, LABEL_Y);
    } else if (isNat1()) {
      canvas.setTextColor(shade(RED, 1));
      canvas.drawString("NAT 1", CENTER_X, LABEL_Y);
    } else if (phase_ != Phase::Rolling) {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("B: zar at", CENTER_X, LABEL_Y);
    }
  }

  // Each sparkle flies out in its own direction at its own speed. Chosen once, when the 20 lands.
  void pickSparkles() {
    for (int i = 0; i < SPARKLE_COUNT; i++) {
      sparkleAngle_[i] = (360.0f * i / SPARKLE_COUNT + random(-12, 12)) * DEG_TO_RAD;
      sparkleSpeed_[i] = random(60, 100) / 100.0f;
    }
  }

  void drawSparkles(LGFX_Sprite& canvas) const {
    float slowDown = 1 - sparkleT_;
    float travel = 1 - slowDown * slowDown;  // ease-out again: fast burst, then drifting
    int size = 1 + int(5 * slowDown);        // they shrink away as they fly
    for (int i = 0; i < SPARKLE_COUNT; i++) {
      float distance = 70 + 110 * sparkleSpeed_[i] * travel;
      int sx = CENTER_X + cosf(sparkleAngle_[i]) * distance;
      int sy = CENTER_Y + sinf(sparkleAngle_[i]) * distance;
      uint16_t color = i % 2 == 0 ? shade(GOLD, 1) : TFT_WHITE;
      // A little four-pointed star: a plus sign with a dot in the middle
      canvas.drawFastHLine(sx - size * 2, sy, size * 4 + 1, color);
      canvas.drawFastVLine(sx, sy - size * 2, size * 4 + 1, color);
      canvas.fillCircle(sx, sy, size / 2 + 1, color);
    }
  }

  static constexpr int SIDES = 20;
  static constexpr Rgb PURPLE = {150, 100, 255};
  static constexpr Rgb GOLD = {255, 200, 50};
  static constexpr Rgb RED = {230, 40, 50};

  static constexpr float CENTER_X = 120;
  static constexpr float CENTER_Y = 108;
  static constexpr float OUTER_RADIUS = 96;  // centre to a corner of the hexagon
  // On a real d20 seen from the front, the front triangle is 0.62x the outline. A bit bigger here, so "20" fits.
  static constexpr float INNER_RADIUS = 62;
  static constexpr int NUMBER_NUDGE_Y = 4;  // the triangle's empty space is a little below its centre
  static constexpr int LABEL_Y = 224;
  // Direction the light comes from: up and to the left (a unit vector)
  static constexpr float LIGHT_X = -0.6f, LIGHT_Y = -0.8f;

  static constexpr uint32_t ROLL_MS = 1100;
  static constexpr uint32_t POP_MS = 350;
  static constexpr uint32_t SHAKE_MS = 700;
  static constexpr float SHAKE_PIXELS = 12;
  static constexpr uint32_t SPARKLE_MS = 1200;
  static constexpr int SPARKLE_COUNT = 14;

  // Which corner points make each visible face and each edge (numbers as in draw()).
  // Seen from the front, a d20 shows 10 of its 20 faces: the front one, 3 next to it and 6 around the outline.
  static constexpr uint8_t FACES[10][3] = {
      {6, 7, 8},                                                        // front
      {6, 7, 1}, {7, 8, 3}, {8, 6, 5},                                  // next to the front
      {6, 5, 0}, {6, 0, 1}, {7, 1, 2}, {7, 2, 3}, {8, 3, 4}, {8, 4, 5}  // around the outline
  };
  static constexpr uint8_t EDGES[18][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0},  // outline
      {6, 7}, {7, 8}, {8, 6},                          // front triangle
      {6, 5}, {6, 0}, {6, 1}, {7, 1}, {7, 2}, {7, 3}, {8, 3}, {8, 4}, {8, 5}  // corners of the triangle to the outline
  };

  Phase phase_ = Phase::Waiting;
  int result_ = 0;
  int shown_ = 0;  // the number on the die right now (changes while rolling), 0 = nothing rolled yet
  int forcedNext_ = 0;
  uint32_t rollStartMs_ = 0;
  uint32_t lastFlipMs_ = 0;
  uint32_t flipGapMs_ = 0;
  uint32_t landedMs_ = 0;

  float spin_ = 0;  // degrees
  float numberScale_ = 1;
  float shakeX_ = 0;
  float sparkleT_ = -1;
  float sparkleAngle_[SPARKLE_COUNT] = {};
  float sparkleSpeed_[SPARKLE_COUNT] = {};
};
