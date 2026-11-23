// Small settings that survive a restart, kept in NVS (a little key-value store in flash).
// Like a sticky note on the device: "last theme: Luffy, brightness: 75%".
#pragma once
#include <Preferences.h>

class Settings {
 public:
  void begin() { store_.begin("pixelcharm", false); }  // false = we will write too

  String themeId() { return store_.getString("theme", "face"); }
  void setThemeId(const String& id) {
    if (id != themeId()) store_.putString("theme", id);  // flash wears out slowly: only write real changes
  }

  uint8_t brightnessLevel() { return store_.getUChar("bright", 3); }
  void setBrightnessLevel(uint8_t level) { store_.putUChar("bright", level); }

 private:
  Preferences store_;
};
