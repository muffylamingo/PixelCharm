// Tasks 1.1-1.3: colour display demo, two debounced buttons and the basic face.
#include <Arduino.h>

#include "button.h"
#include "face.h"
#include "lgfx_config.h"

LGFX display;
// An invisible 240x240 drawing board in RAM. Each frame is drawn here first and then sent to
// the display in one go, so the display never shows a half-drawn frame (no flicker).
LGFX_Sprite canvas(&display);
Button buttonA(4);  // pin plan: button A on GPIO 4
Button buttonB(5);  // pin plan: button B on GPIO 5
Face face;

// Our real screen is 240x240. Wokwi's ILI9341 is 240x320, so we only use the top 240 rows.
constexpr int SCREEN_SIZE = 240;
constexpr int HEADER_HEIGHT = 80;  // title, FPS and the row of shapes
constexpr int BALL_RADIUS = 10;

// A short press goes through these in order: face -> demo -> button test -> face...
enum class Screen { Face, Demo, ButtonTest };
Screen currentScreen = Screen::Face;

float ballX = 120, ballY = 160;
float ballSpeedX = 90, ballSpeedY = 70;  // pixels per second
constexpr uint16_t BALL_COLORS[] = {TFT_ORANGE, TFT_CYAN, TFT_MAGENTA, TFT_GREENYELLOW};
int ballColor = 0;

// The real backlight is wired up in task 1.7. Until then the level is only shown on screen.
constexpr int BRIGHTNESS_LEVELS[] = {25, 50, 75, 100};
int brightness = 3;

struct PressCounts {
  int shortPresses = 0;
  int longPresses = 0;
};
PressCounts countsA, countsB;
uint32_t lastPressMs = 0;

uint32_t lastFrameMs = 0;
uint32_t fpsWindowStartMs = 0;
uint32_t framesInWindow = 0;
float fps = 0;

// ---------- Demo screen (task 1.1) ----------

void moveBall(float seconds) {
  // Speed is in pixels per second, so the ball moves at the same speed whatever the FPS is
  ballX += ballSpeedX * seconds;
  ballY += ballSpeedY * seconds;

  const float minX = BALL_RADIUS, maxX = SCREEN_SIZE - 1 - BALL_RADIUS;
  const float minY = HEADER_HEIGHT + BALL_RADIUS, maxY = SCREEN_SIZE - 1 - BALL_RADIUS;
  if (ballX < minX || ballX > maxX) {
    ballX = constrain(ballX, minX, maxX);
    ballSpeedX = -ballSpeedX;
  }
  if (ballY < minY || ballY > maxY) {
    ballY = constrain(ballY, minY, maxY);
    ballSpeedY = -ballSpeedY;
  }
}

void drawDemoScreen() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(2);
  canvas.drawString("PixelCharm", 6, 8);

  char text[16];
  snprintf(text, sizeof(text), "%5.1f FPS", fps);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextSize(1);
  canvas.drawString(text, 180, 12);

  // One of each basic shape
  canvas.fillRect(10, 36, 36, 36, TFT_RED);
  canvas.fillCircle(78, 54, 18, TFT_GREEN);
  canvas.fillTriangle(110, 72, 128, 36, 146, 72, TFT_BLUE);
  canvas.fillRoundRect(160, 36, 36, 36, 8, TFT_YELLOW);
  canvas.drawCircle(218, 54, 16, TFT_MAGENTA);

  canvas.drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_SIZE, TFT_DARKGREY);
  canvas.fillCircle(ballX, ballY, BALL_RADIUS, BALL_COLORS[ballColor]);
}

// ---------- Button test screen (task 1.2) ----------

void drawLine(int y, const char* text) {
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(2);
  canvas.drawString(text, 10, y);
}

// Fills up while the button is held; turns green once it counts as a long press
void drawHoldBar(int y, uint32_t heldMs) {
  constexpr int BAR_X = 10, BAR_WIDTH = 220, BAR_HEIGHT = 6;
  int filled = BAR_WIDTH * min(heldMs, Button::LONG_PRESS_MS) / Button::LONG_PRESS_MS;
  uint16_t color = heldMs >= Button::LONG_PRESS_MS ? TFT_GREEN : TFT_YELLOW;
  canvas.fillRect(BAR_X, y, filled, BAR_HEIGHT, color);
  canvas.fillRect(BAR_X + filled, y, BAR_WIDTH - filled, BAR_HEIGHT, TFT_DARKGREY);
}

void drawButtonTestScreen(uint32_t now) {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setTextSize(2);
  canvas.drawString("Buton testi", 10, 10);

  char text[24];
  snprintf(text, sizeof(text), "A  kisa: %3d", countsA.shortPresses);
  drawLine(40, text);
  snprintf(text, sizeof(text), "   uzun: %3d", countsA.longPresses);
  drawLine(60, text);
  drawHoldBar(84, buttonA.heldMs(now));
  snprintf(text, sizeof(text), "B  kisa: %3d", countsB.shortPresses);
  drawLine(104, text);
  snprintf(text, sizeof(text), "   uzun: %3d", countsB.longPresses);
  drawLine(124, text);
  drawHoldBar(148, buttonB.heldMs(now));
  snprintf(text, sizeof(text), "Son: %5lu ms", (unsigned long)lastPressMs);
  drawLine(168, text);
  snprintf(text, sizeof(text), "Parlaklik: %3d%%", BRIGHTNESS_LEVELS[brightness]);
  drawLine(190, text);

  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.setTextSize(1);
  canvas.drawString("A kisa=ekran  A uzun=parlaklik", 10, 218);
  canvas.drawString("B kisa=eylem  cubuk=basili tutma", 10, 228);
}

// ---------- Buttons ----------

Screen nextScreen(Screen screen) {
  switch (screen) {
    case Screen::Face:
      return Screen::Demo;
    case Screen::Demo:
      return Screen::ButtonTest;
    default:
      return Screen::Face;
  }
}

void logPress(char name, ButtonEvent event, uint32_t ms) {
  Serial.printf("%c %s basis (%lu ms)\n", name, event == ButtonEvent::LongPress ? "uzun" : "kisa", (unsigned long)ms);
}

void handleButtons(uint32_t now) {
  ButtonEvent a = buttonA.update(now);
  ButtonEvent b = buttonB.update(now);

  if (a != ButtonEvent::None) {
    lastPressMs = buttonA.lastPressMs();
    logPress('A', a, lastPressMs);
    if (a == ButtonEvent::ShortPress) {
      countsA.shortPresses++;
      currentScreen = nextScreen(currentScreen);
    } else {
      countsA.longPresses++;
      brightness = (brightness + 1) % 4;
    }
  }

  if (b != ButtonEvent::None) {
    lastPressMs = buttonB.lastPressMs();
    logPress('B', b, lastPressMs);
    if (b == ButtonEvent::ShortPress) {
      countsB.shortPresses++;
      // Each screen's "action"
      if (currentScreen == Screen::Face) face.react(now);
      if (currentScreen == Screen::Demo) ballColor = (ballColor + 1) % 4;
    } else {
      countsB.longPresses++;  // B's long presses get their jobs in Faz 2 and 4 (see the button map)
    }
  }
}

// Counts the frames sent to the display and works out the FPS once a second
void updateFps(uint32_t now) {
  framesInWindow++;
  if (now - fpsWindowStartMs < 1000) return;

  fps = framesInWindow * 1000.0f / (now - fpsWindowStartMs);
  framesInWindow = 0;
  fpsWindowStartMs = now;
}

// ---------- Setup and main loop ----------

void setup() {
  Serial.begin(115200);
  buttonA.begin();
  buttonB.begin();
  display.init();
  display.fillScreen(TFT_BLACK);

  // Only exists in Wokwi: shade the rows our real 240x240 screen does not have
  display.fillRect(0, SCREEN_SIZE, display.width(), display.height() - SCREEN_SIZE, TFT_DARKGREY);
  display.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
  display.setTextSize(1);
  display.drawString("Wokwi only: outside the 240x240 area", 8, SCREEN_SIZE + 8);

  // 240 x 240 pixels x 2 bytes = 115,200 bytes of RAM
  canvas.setColorDepth(16);
  if (canvas.createSprite(SCREEN_SIZE, SCREEN_SIZE)) {
    Serial.printf("Sprite hazir: %lu bayt. Kalan RAM: %lu bayt\n", (unsigned long)canvas.bufferLength(),
                  (unsigned long)ESP.getFreeHeap());
  } else {
    Serial.println("HATA: sprite icin yeterli RAM yok");
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.drawString("Sprite icin RAM yetmedi", 8, 8);
  }

  lastFrameMs = fpsWindowStartMs = millis();
  Serial.println("Merhaba PixelCharm! Ekran ve butonlar hazir.");
}

void loop() {
  uint32_t now = millis();
  float seconds = (now - lastFrameMs) / 1000.0f;  // time since the previous frame
  lastFrameMs = now;

  handleButtons(now);

  // Draw the current screen into the sprite...
  switch (currentScreen) {
    case Screen::Face:
      face.update(now, seconds);
      face.draw(canvas);
      break;
    case Screen::Demo:
      moveBall(seconds);
      drawDemoScreen();
      break;
    case Screen::ButtonTest:
      drawButtonTestScreen(now);
      break;
  }
  // ...then send the finished frame to the display in one go
  canvas.pushSprite(0, 0);
  updateFps(now);
}
