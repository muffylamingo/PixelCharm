// The picture themes: 240x240 JPEG files on the board's LittleFS "disk", in the /themes folder.
// tools/prepare_images.py makes them; "Upload Filesystem Image" copies firmware/data/ onto the board.
#pragma once
#include <JPEGDEC.h>
#include <LittleFS.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include <algorithm>
#include <memory>
#include <vector>

class Gallery {
 public:
  // Opens the disk and lists the pictures in /themes, sorted by name (01_..., 02_...). Returns how many it found.
  size_t begin() {
    // false = if there is no disk yet, don't format one: better to see the message below
    if (!LittleFS.begin(false, "/littlefs", 5, "littlefs")) {
      Serial.println("LittleFS acilamadi: once 'Upload Filesystem Image' ile resimleri yukle");
      return 0;
    }
    File folder = LittleFS.open(FOLDER);
    for (File file = folder.openNextFile(); file; file = folder.openNextFile()) {
      String name = file.name();
      if (name.endsWith(".jpg")) paths_.push_back(String(FOLDER) + "/" + name);
    }
    std::sort(paths_.begin(), paths_.end());
    Serial.printf("LittleFS: %u resim, disk %u / %u KB dolu\n", (unsigned)paths_.size(), (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
    return paths_.size();
  }

  size_t count() const { return paths_.size(); }

  // Decodes picture `index` straight into the canvas and reports how long it took.
  // A picture doesn't move, so this runs once when it comes on screen, not every frame.
  void draw(LGFX_Sprite& canvas, size_t index) const {
    canvas.fillScreen(TFT_BLACK);
    if (index >= paths_.size()) {
      showError(canvas, "Resim yok");
      return;
    }
    // Two steps, timed separately: read the whole file into RAM, then decode it into the canvas
    uint32_t startUs = micros();
    File file = LittleFS.open(paths_[index]);
    std::vector<uint8_t> jpeg(file.size());
    size_t got = file.read(jpeg.data(), jpeg.size());
    file.close();
    uint32_t readUs = micros() - startUs;

    // The decoder needs ~17 KB of working memory: borrow it only while decoding
    std::unique_ptr<JPEGDEC> decoder(new JPEGDEC);
    bool ok = got == jpeg.size() && decoder->openRAM(jpeg.data(), jpeg.size(), copyBlock);
    if (ok) {
      decoder->setUserPointer(&canvas);
      // Big-endian RGB565 is exactly how the canvas stores its pixels, so blocks can be copied as they are
      decoder->setPixelType(RGB565_BIG_ENDIAN);
      ok = decoder->decode(0, 0, 0) == 1;
      decoder->close();
    }
    uint32_t decodeUs = micros() - startUs - readUs;
    Serial.printf("Resim %u/%u %s (%u KB): %s, okuma %.1f ms + cozme %.1f ms\n", (unsigned)(index + 1), (unsigned)paths_.size(),
                  paths_[index].c_str(), (unsigned)(jpeg.size() / 1024), ok ? "cizildi" : "HATA", readUs / 1000.0f,
                  decodeUs / 1000.0f);
    if (!ok) showError(canvas, "JPEG okunamadi");
  }

 private:
  // The decoder hands over the picture a few blocks at a time; each block goes into its place in the canvas
  static int copyBlock(JPEGDRAW* block) {
    auto* canvas = static_cast<LGFX_Sprite*>(block->pUser);
    canvas->pushImage(block->x, block->y, block->iWidth, block->iHeight, reinterpret_cast<lgfx::swap565_t*>(block->pPixels));
    return 1;  // 1 = keep going
  }

  static void showError(LGFX_Sprite& canvas, const char* text) {
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(2);
    canvas.drawString(text, 10, 10);
  }

  static constexpr const char* FOLDER = "/themes";
  std::vector<String> paths_;
};
