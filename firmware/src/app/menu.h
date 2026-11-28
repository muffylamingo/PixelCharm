// The theme menu: A long opens it, A short moves to the next square, B opens that square, A long closes it.
// The squares are every theme's icon, then brightness, then the test screen. Nine squares to a page.
#pragma once
#include "../themes/catalog.h"

class Menu {
 public:
  enum class Item { Theme, Brightness, Test };

  void open(const ThemeCatalog& themes, size_t selected, uint32_t now) {
    themes_ = &themes;
    selected_ = selected;
    changed(now);
  }
  void next(uint32_t now) {
    selected_ = (selected_ + 1) % itemCount();
    changed(now);
  }
  // Something on the page changed (the brightness square): draw it again
  void refresh(uint32_t now) { changed(now); }

  Item selectedItem() const {
    if (selected_ < themes_->count()) return Item::Theme;
    return selected_ == themes_->count() ? Item::Brightness : Item::Test;
  }
  size_t selectedTheme() const { return selected_; }
  // Nobody touched anything for a while: go back to the theme
  bool timedOut(uint32_t now) const { return now - lastChangeMs_ >= TIMEOUT_MS; }

  // Draws only after a change. The icons are small JPEGs: no need to decode them 40 times a second.
  void draw(LGFX_Sprite& canvas, int brightnessPercent) {
    if (!dirty_) return;
    dirty_ = false;
    uint32_t startUs = micros();
    canvas.fillScreen(TFT_BLACK);
    size_t first = selected_ / PER_PAGE * PER_PAGE;
    size_t last = min(first + PER_PAGE, itemCount());
    for (size_t i = first; i < last; i++) {
      int cell = i - first;
      int x = cell % COLUMNS * CELL_WIDTH + (CELL_WIDTH - ICON) / 2;
      int y = cell / COLUMNS * CELL_HEIGHT + (CELL_HEIGHT - ICON) / 2;
      drawItem(canvas, i, x, y, brightnessPercent);
      if (i == selected_) {
        // A two-pixel frame around the chosen square
        canvas.drawRoundRect(x - 4, y - 4, ICON + 8, ICON + 8, 8, HIGHLIGHT);
        canvas.drawRoundRect(x - 3, y - 3, ICON + 6, ICON + 6, 7, HIGHLIGHT);
      }
    }
    drawNameBar(canvas);
    Serial.printf("Menu: %s (%.0f ms)\n", itemName(selected_).c_str(), (micros() - startUs) / 1000.0f);
  }

 private:
  size_t itemCount() const { return themes_->count() + 2; }  // + brightness + test

  String itemName(size_t i) const {
    if (i < themes_->count()) return (*themes_)[i].name();
    return i == themes_->count() ? "Parlaklik" : "Test";
  }

  void changed(uint32_t now) {
    lastChangeMs_ = now;
    dirty_ = true;
  }

  void drawItem(LGFX_Sprite& canvas, size_t i, int x, int y, int brightnessPercent) const {
    if (i < themes_->count()) {
      (*themes_)[i].drawIcon(canvas, x, y, ICON);
    } else if (i == themes_->count()) {
      drawBrightnessIcon(canvas, x, y, brightnessPercent);
    } else {
      drawTestIcon(canvas, x, y);
    }
  }

  // A sun and the current level
  static void drawBrightnessIcon(LGFX_Sprite& canvas, int x, int y, int percent) {
    canvas.fillRoundRect(x, y, ICON, ICON, 8, TILE);
    int centerX = x + ICON / 2, centerY = y + 26;
    uint16_t sun = lgfx::color565(255, 210, 80);
    canvas.fillCircle(centerX, centerY, 9, sun);
    for (int ray = 0; ray < 8; ray++) {
      float angle = ray * 45 * DEG_TO_RAD;
      canvas.drawWideLine(centerX + cosf(angle) * 13, centerY + sinf(angle) * 13, centerX + cosf(angle) * 18,
                          centerY + sinf(angle) * 18, 1.2f, sun);
    }
    char text[8];
    snprintf(text, sizeof(text), "%%%d", percent);  // Turkish writes the percent sign first: %75
    drawCenteredText(canvas, text, centerX, y + 54, TFT_WHITE);
  }

  // Two hold bars, like on the test screen
  static void drawTestIcon(LGFX_Sprite& canvas, int x, int y) {
    canvas.fillRoundRect(x, y, ICON, ICON, 8, TILE);
    drawCenteredText(canvas, "TEST", x + ICON / 2, y + 18, TFT_YELLOW);
    canvas.fillRect(x + 10, y + 34, 44, 6, TFT_GREEN);
    canvas.fillRect(x + 10, y + 46, 28, 6, TFT_YELLOW);
    canvas.fillRect(x + 38, y + 46, 16, 6, TFT_DARKGREY);
  }

  void drawNameBar(LGFX_Sprite& canvas) const {
    canvas.fillRect(0, GRID_HEIGHT, 240, 240 - GRID_HEIGHT, lgfx::color565(30, 30, 40));
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(itemName(selected_), 120, GRID_HEIGHT + 12);
    size_t pages = (itemCount() + PER_PAGE - 1) / PER_PAGE;
    if (pages > 1) {
      char text[24];
      snprintf(text, sizeof(text), "%u/%u", (unsigned)(selected_ / PER_PAGE + 1), (unsigned)pages);
      canvas.setFont(&fonts::Font0);
      canvas.setTextDatum(textdatum_t::middle_right);
      canvas.setTextColor(TFT_LIGHTGREY);
      canvas.drawString(text, 234, GRID_HEIGHT + 12);
    }
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(textdatum_t::top_left);
  }

  static void drawCenteredText(LGFX_Sprite& canvas, const char* text, int x, int y, uint16_t color) {
    canvas.setFont(&fonts::Font2);
    canvas.setTextSize(1);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(color);
    canvas.drawString(text, x, y);
    canvas.setFont(&fonts::Font0);
    canvas.setTextDatum(textdatum_t::top_left);
  }

  static constexpr int COLUMNS = 3;
  static constexpr size_t PER_PAGE = 9;
  static constexpr int CELL_WIDTH = 80, CELL_HEIGHT = 72;
  static constexpr int GRID_HEIGHT = 3 * CELL_HEIGHT;  // 216; the name bar fills the last 24 rows
  static constexpr int ICON = 64;                      // the thumbnails from tools/prepare_images.py are 64x64
  static constexpr uint32_t TIMEOUT_MS = 20000;
  static constexpr uint16_t HIGHLIGHT = lgfx::color565(90, 220, 255);  // the face's cyan
  static constexpr uint16_t TILE = lgfx::color565(40, 40, 55);

  const ThemeCatalog* themes_ = nullptr;
  size_t selected_ = 0;
  uint32_t lastChangeMs_ = 0;
  bool dirty_ = true;
};
