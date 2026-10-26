// Task 1.1: first drawings on a colour display with LovyanGFX.
#include <Arduino.h>

#include "lgfx_config.h"

LGFX display;

// Our real screen is 240x240. Wokwi's ILI9341 is 240x320, so we only use the top 240 rows.
constexpr int SCREEN_SIZE = 240;
constexpr int HEADER_HEIGHT = 80;  // title, FPS and the row of shapes
constexpr int BALL_RADIUS = 10;

float ballX = 120, ballY = 160;
float ballSpeedX = 90, ballSpeedY = 70;  // pixels per second

uint32_t lastFrameMs = 0;
uint32_t fpsWindowStartMs = 0;
uint32_t framesInWindow = 0;

void drawHeader() {
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

void drawOutsideArea() {
  // Only exists in Wokwi: shade the rows our real 240x240 screen does not have
  display.fillRect(0, SCREEN_SIZE, display.width(), display.height() - SCREEN_SIZE, TFT_DARKGREY);
  display.setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
  display.setTextSize(1);
  display.drawString("Wokwi only: outside the 240x240 area", 8, SCREEN_SIZE + 8);
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

  display.fillCircle(ballX, ballY, BALL_RADIUS, TFT_ORANGE);
}

void updateFps(uint32_t now) {
  framesInWindow++;
  if (now - fpsWindowStartMs < 1000) return;

  float fps = framesInWindow * 1000.0f / (now - fpsWindowStartMs);
  char text[16];
  snprintf(text, sizeof(text), "%5.1f FPS", fps);  // fixed width, so the old text is fully overwritten
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setTextSize(1);
  display.drawString(text, 180, 12);
  Serial.printf("FPS: %.1f\n", fps);

  framesInWindow = 0;
  fpsWindowStartMs = now;
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.fillScreen(TFT_BLACK);
  drawHeader();
  drawOutsideArea();
  lastFrameMs = fpsWindowStartMs = millis();
  Serial.println("Merhaba PixelCharm! Ekran hazir.");
}

void loop() {
  uint32_t now = millis();
  float seconds = (now - lastFrameMs) / 1000.0f;  // time since the previous frame
  lastFrameMs = now;

  moveBall(seconds);
  updateFps(now);
}
