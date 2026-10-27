// Tasks 1.1-1.2: colour display demo and two debounced buttons.
#include <Arduino.h>

#include "button.h"
#include "lgfx_config.h"

LGFX display;
Button buttonA(4);  // pin plan: button A on GPIO 4
Button buttonB(5);  // pin plan: button B on GPIO 5

// Our real screen is 240x240. Wokwi's ILI9341 is 240x320, so we only use the top 240 rows.
constexpr int SCREEN_SIZE = 240;
constexpr int HEADER_HEIGHT = 80;  // title, FPS and the row of shapes
constexpr int BALL_RADIUS = 10;

enum class Screen { Demo, ButtonTest };
Screen currentScreen = Screen::Demo;

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

// ---------- Demo screen (task 1.1) ----------

void drawDemoScreen() {
  display.fillRect(0, 0, SCREEN_SIZE, SCREEN_SIZE, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("PixelCharm", 6, 8);

  // One of each basic shape
  display.fillRect(10, 36, 36, 36, TFT_RED);
  display.fillCircle(78, 54, 18, TFT_GREEN);
  display.fillTriangle(110, 72, 128, 36, 146, 72, TFT_BLUE);
  display.fillRoundRect(160, 36, 36, 36, 8, TFT_YELLOW);
  display.drawCircle(218, 54, 16, TFT_MAGENTA);

  display.drawFastHLine(0, HEADER_HEIGHT - 1, SCREEN_SIZE, TFT_DARKGREY);
}

void moveBall(float seconds) {
  // Erase the ball at its old position, move it, then draw it again
  display.fillCircle(ballX, ballY, BALL_RADIUS, TFT_BLACK);

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

  display.fillCircle(ballX, ballY, BALL_RADIUS, BALL_COLORS[ballColor]);
}

void updateFps(uint32_t now) {
  framesInWindow++;
  if (now - fpsWindowStartMs < 1000) return;

  float fps = framesInWindow * 1000.0f / (now - fpsWindowStartMs);
  if (currentScreen == Screen::Demo) {
    char text[16];
    snprintf(text, sizeof(text), "%5.1f FPS", fps);  // fixed width, so the old text is fully overwritten
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextSize(1);
    display.drawString(text, 180, 12);
  }
  framesInWindow = 0;
  fpsWindowStartMs = now;
}

// ---------- Button test screen (task 1.2) ----------

void drawLine(int y, const char* text) {
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(2);
  display.drawString(text, 10, y);
}

void drawButtonTestValues() {
  char text[24];
  snprintf(text, sizeof(text), "A  kisa: %3d", countsA.shortPresses);
  drawLine(40, text);
  snprintf(text, sizeof(text), "   uzun: %3d", countsA.longPresses);
  drawLine(60, text);
  snprintf(text, sizeof(text), "B  kisa: %3d", countsB.shortPresses);
  drawLine(104, text);
  snprintf(text, sizeof(text), "   uzun: %3d", countsB.longPresses);
  drawLine(124, text);
  snprintf(text, sizeof(text), "Son: %5lu ms", (unsigned long)lastPressMs);
  drawLine(168, text);
  snprintf(text, sizeof(text), "Parlaklik: %3d%%", BRIGHTNESS_LEVELS[brightness]);
  drawLine(190, text);
}

void drawButtonTestScreen() {
  display.fillRect(0, 0, SCREEN_SIZE, SCREEN_SIZE, TFT_BLACK);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setTextSize(2);
  display.drawString("Buton testi", 10, 10);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setTextSize(1);
  display.drawString("A kisa=ekran  A uzun=parlaklik", 10, 218);
  display.drawString("B kisa=eylem  cubuk=basili tutma", 10, 228);
  drawButtonTestValues();
}

// Fills up while the button is held; turns green once it counts as a long press
void drawHoldBar(int y, uint32_t heldMs) {
  constexpr int BAR_X = 10, BAR_WIDTH = 220, BAR_HEIGHT = 6;
  int filled = BAR_WIDTH * min(heldMs, Button::LONG_PRESS_MS) / Button::LONG_PRESS_MS;
  uint16_t color = heldMs >= Button::LONG_PRESS_MS ? TFT_GREEN : TFT_YELLOW;
  display.fillRect(BAR_X, y, filled, BAR_HEIGHT, color);
  display.fillRect(BAR_X + filled, y, BAR_WIDTH - filled, BAR_HEIGHT, TFT_DARKGREY);
}

// ---------- Buttons ----------

void showScreen(Screen screen) {
  currentScreen = screen;
  if (screen == Screen::Demo) {
    drawDemoScreen();
  } else {
    drawButtonTestScreen();
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
      showScreen(currentScreen == Screen::Demo ? Screen::ButtonTest : Screen::Demo);
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
      if (currentScreen == Screen::Demo) ballColor = (ballColor + 1) % 4;  // the demo screen's "action"
    } else {
      countsB.longPresses++;  // B's long presses get their jobs in Faz 2 and 4 (see the button map)
    }
  }

  if (currentScreen == Screen::ButtonTest) {
    if (a != ButtonEvent::None || b != ButtonEvent::None) drawButtonTestValues();
    drawHoldBar(84, buttonA.heldMs(now));
    drawHoldBar(148, buttonB.heldMs(now));
  }
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

  showScreen(Screen::Demo);
  lastFrameMs = fpsWindowStartMs = millis();
  Serial.println("Merhaba PixelCharm! Ekran ve butonlar hazir.");
}

void loop() {
  uint32_t now = millis();
  float seconds = (now - lastFrameMs) / 1000.0f;  // time since the previous frame
  lastFrameMs = now;

  handleButtons(now);
  if (currentScreen == Screen::Demo) moveBall(seconds);
  updateFps(now);
}
