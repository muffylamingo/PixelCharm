// The list of every theme the device can show: the built-in face and d20 first, then the pictures and GIFs
// listed in /themes/index.json on the LittleFS disk. In Faz 2 the phone app will write that file.
#pragma once
#include <ArduinoJson.h>
#include <LittleFS.h>

#include <memory>
#include <vector>

#include "dice.h"
#include "face.h"
#include "picture.h"

class ThemeCatalog {
 public:
  void load() {
    themes_.clear();
    themes_.emplace_back(new Face);
    dice_ = new Dice;
    themes_.emplace_back(dice_);
    loadPictures();
  }

  size_t count() const { return themes_.size(); }
  Theme& operator[](size_t index) { return *themes_[index]; }
  const Theme& operator[](size_t index) const { return *themes_[index]; }

  // Where the theme with this id is in the list. A theme that is gone (deleted picture) falls back to the face.
  size_t find(const String& id) const {
    for (size_t i = 0; i < themes_.size(); i++) {
      if (themes_[i]->id() == id) return i;
    }
    return 0;
  }

  Dice& dice() { return *dice_; }  // for the d20's Serial test commands

 private:
  void loadPictures() {
    // false = if there is no disk yet, don't format one: better to see the message below
    if (!LittleFS.begin(false, "/littlefs", 5, "littlefs")) {
      Serial.println("LittleFS acilamadi: once 'Upload Filesystem Image' ile resimleri yukle");
      return;
    }
    File file = LittleFS.open(INDEX_PATH);
    if (!file) {
      Serial.printf("%s yok: sadece yuz ve zar var\n", INDEX_PATH);
      return;
    }
    // index.json: {"version": 1, "themes": [{"name": "Luffy", "type": "image", "file": "/themes/...", "thumb": "..."}]}
    JsonDocument index;
    DeserializationError error = deserializeJson(index, file);
    if (error) {
      Serial.printf("%s okunamadi: %s\n", INDEX_PATH, error.c_str());
      return;
    }
    for (JsonObject entry : index["themes"].as<JsonArray>()) {
      String type = entry["type"] | "";
      String name = entry["name"] | "Resim";
      String path = entry["file"] | "";
      String thumb = entry["thumb"] | "";
      if (!LittleFS.exists(path)) {
        Serial.printf("Tema atlandi, dosya yok: %s\n", path.c_str());
      } else if (type == "image") {
        themes_.emplace_back(new ImageTheme(name, path, thumb));
      } else if (type == "gif") {
        themes_.emplace_back(new GifTheme(name, path, thumb));
      } else {
        Serial.printf("Tema atlandi, bilinmeyen tur: %s\n", type.c_str());
      }
    }
    Serial.printf("Temalar: %u (2 hazir + %u resim). Disk %u / %u KB dolu\n", (unsigned)themes_.size(),
                  (unsigned)(themes_.size() - 2), (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
  }

  static constexpr const char* INDEX_PATH = "/themes/index.json";
  std::vector<std::unique_ptr<Theme>> themes_;
  Dice* dice_ = nullptr;  // also owned by themes_
};
