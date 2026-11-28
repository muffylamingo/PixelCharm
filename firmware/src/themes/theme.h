// What every theme can do. The app only talks to themes through this, so it doesn't need to know
// whether it is showing the face, the d20, a picture or a GIF. A new kind of theme just implements these.
#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class Theme {
 public:
  virtual ~Theme() = default;

  // Short and stable, saved in NVS to come back to this theme after a restart ("face", "dice" or the file path)
  virtual String id() const = 0;
  // The name the menu shows (ASCII: the fonts have no ç, ğ, ı...)
  virtual String name() const = 0;

  // The theme comes on screen. A still picture draws itself here, once.
  virtual void enter(LGFX_Sprite& canvas, uint32_t now) {}
  // Every frame while it is on screen. Moving themes redraw the canvas here.
  virtual void frame(LGFX_Sprite& canvas, uint32_t now, float seconds) = 0;
  // B short press: roll the die, make the face smile...
  virtual void action(uint32_t now) {}
  // The theme goes off screen (another theme or the menu takes the canvas): give back any memory it borrowed
  virtual void leave() {}

  // A small square picture of the theme for the menu, `size` pixels wide, top left at (x, y)
  virtual void drawIcon(LGFX_Sprite& canvas, int x, int y, int size) const = 0;
};
