// PixelCharm firmware. Everything happens in app/app.h; the folders:
//   app/      the states (boot, theme, menu, test), settings in NVS, the menu
//   themes/   face, d20, pictures and GIFs, all behind one Theme interface
//   input/    buttons (push or touch)
//   display/  which display and how it is wired
//   board_devkit.h  every pin in one place
#include <Arduino.h>

#include "app/app.h"

App app;

void setup() { app.begin(); }

void loop() { app.loop(); }
