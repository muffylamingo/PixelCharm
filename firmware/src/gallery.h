// The picture themes: 240x240 JPEGs and animated GIFs on the board's LittleFS "disk", in the /themes folder.
// tools/prepare_images.py makes them; "Upload Filesystem Image" copies firmware/data/ onto the board.
#pragma once
#include <AnimatedGIF.h>
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
      if (name.endsWith(".jpg") || name.endsWith(".gif")) paths_.push_back(String(FOLDER) + "/" + name);
    }
    std::sort(paths_.begin(), paths_.end());
    Serial.printf("LittleFS: %u resim, disk %u / %u KB dolu\n", (unsigned)paths_.size(), (unsigned)(LittleFS.usedBytes() / 1024),
                  (unsigned)(LittleFS.totalBytes() / 1024));
    return paths_.size();
  }

  size_t count() const { return paths_.size(); }

  // Puts picture `index` on the canvas: a JPEG is drawn once, a GIF starts playing (see update())
  void show(LGFX_Sprite& canvas, size_t index) {
    stop();
    canvas.fillScreen(TFT_BLACK);
    if (index >= paths_.size()) {
      showError(canvas, "Resim yok");
      return;
    }
    const String& path = paths_[index];
    Serial.printf("Resim %u/%u %s: ", (unsigned)(index + 1), (unsigned)paths_.size(), path.c_str());
    if (path.endsWith(".gif")) {
      startGif(canvas, path);
    } else {
      drawJpeg(canvas, path);
    }
  }

  // Call every frame while the picture screen is on: moves a GIF on to its next frame when it's time
  void update(LGFX_Sprite& canvas, uint32_t now) {
    if (!gif_ || now - frameShownMs_ < frameDelayMs_) return;
    // Keep to the GIF's own timing: the next frame is due one delay after the previous one was due.
    // If we fell far behind, start counting from now instead of rushing to catch up.
    frameShownMs_ += frameDelayMs_;
    if (now - frameShownMs_ > frameDelayMs_) frameShownMs_ = now;
    nextGifFrame(canvas);
  }

  // Leaving the picture screen: give back the GIF's memory
  void stop() {
    if (!gif_) return;
    gif_->close();
    gif_.reset();
    free(gifFrameBuffer_);
    free(gifFile_);
    gifFrameBuffer_ = gifFile_ = nullptr;
  }

  ~Gallery() { stop(); }

 private:
  // Reads the whole file into RAM (`psram` = the big, slower 8 MB RAM). Returns nullptr on failure.
  static uint8_t* readFile(const String& path, size_t& size, bool psram) {
    File file = LittleFS.open(path);
    size = file.size();
    auto* data = static_cast<uint8_t*>(psram ? ps_malloc(size) : malloc(size));
    if (data && file.read(data, size) != size) {
      free(data);
      data = nullptr;
    }
    return data;
  }

  void drawJpeg(LGFX_Sprite& canvas, const String& path) {
    // Two steps, timed separately: read the whole file into RAM, then decode it into the canvas
    uint32_t startUs = micros();
    size_t size;
    std::unique_ptr<uint8_t, decltype(&free)> jpeg(readFile(path, size, false), &free);
    uint32_t readUs = micros() - startUs;

    // The decoder needs ~17 KB of working memory: borrow it only while decoding
    std::unique_ptr<JPEGDEC> decoder(new JPEGDEC);
    bool ok = jpeg && decoder->openRAM(jpeg.get(), size, copyJpegBlock);
    if (ok) {
      decoder->setUserPointer(&canvas);
      // Big-endian RGB565 is exactly how the canvas stores its pixels, so blocks can be copied as they are
      decoder->setPixelType(RGB565_BIG_ENDIAN);
      ok = decoder->decode(0, 0, 0) == 1;
      decoder->close();
    }
    uint32_t decodeUs = micros() - startUs - readUs;
    Serial.printf("JPEG %u KB, %s, okuma %.1f ms + cozme %.1f ms\n", (unsigned)(size / 1024), ok ? "cizildi" : "HATA",
                  readUs / 1000.0f, decodeUs / 1000.0f);
    if (!ok) showError(canvas, "JPEG okunamadi");
  }

  // The JPEG decoder hands over the picture a few blocks at a time; each block goes into its place in the canvas
  static int copyJpegBlock(JPEGDRAW* block) {
    auto* canvas = static_cast<LGFX_Sprite*>(block->pUser);
    canvas->pushImage(block->x, block->y, block->iWidth, block->iHeight, reinterpret_cast<lgfx::swap565_t*>(block->pPixels));
    return 1;  // 1 = keep going
  }

  void startGif(LGFX_Sprite& canvas, const String& path) {
    uint32_t startUs = micros();
    size_t size;
    // A GIF can be 1 MB: it goes into PSRAM, and frames are decoded from there
    gifFile_ = readFile(path, size, true);
    uint32_t readUs = micros() - startUs;
    gif_.reset(new AnimatedGIF);
    gif_->begin(GIF_PALETTE_RGB565_BE);  // the canvas's own pixel format
    if (!gifFile_ || !gif_->open(gifFile_, size, copyGifLine)) {
      Serial.println("GIF acilamadi");
      stop();
      showError(canvas, "GIF okunamadi");
      return;
    }
    // "Cooked" mode: the library keeps the whole picture as 8-bit colour numbers (frames can change only a part
    // of it) and hands us finished RGB565 lines. It needs width x (height + 2) bytes for that.
    int width = gif_->getCanvasWidth(), height = gif_->getCanvasHeight();
    gifFrameBuffer_ = static_cast<uint8_t*>(malloc(width * (height + 2)));
    if (!gifFrameBuffer_) {
      Serial.println("GIF icin RAM yetmedi");
      stop();
      showError(canvas, "RAM yetmedi");
      return;
    }
    gif_->setDrawType(GIF_DRAW_COOKED);
    gif_->setFrameBuf(gifFrameBuffer_);
    GIFINFO info;
    gif_->getInfo(&info);
    Serial.printf("GIF %ux%u, %u KB, %d kare, %.2f sn, okuma %.1f ms. Kalan RAM %u KB, PSRAM %u KB\n", width, height,
                  (unsigned)(size / 1024), (int)info.iFrameCount, info.iDuration / 1000.0f, readUs / 1000.0f,
                  (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
    frameNumber_ = 0;
    slowestFrameUs_ = totalFrameUs_ = 0;
    loopStartMs_ = millis();
    nextGifFrame(canvas);
    frameShownMs_ = millis();
  }

  void nextGifFrame(LGFX_Sprite& canvas) {
    uint32_t startUs = micros();
    int delayMs = 100;
    // 1 = a frame was drawn and more follow, 0 = that was the last frame, -1 = error
    int result = gif_->playFrame(false, &delayMs, &canvas);
    uint32_t frameUs = micros() - startUs;
    frameDelayMs_ = delayMs > 0 ? delayMs : 100;
    frameNumber_++;
    totalFrameUs_ += frameUs;
    slowestFrameUs_ = max(slowestFrameUs_, frameUs);
    if (result != 1) {
      // The last frame: report how fast decoding was, then start again from the first frame
      uint32_t now = millis();
      Serial.printf("GIF dongusu: %d kare %lu ms'de, cozme ortalama %.1f ms, en uzun %.1f ms\n", frameNumber_,
                    (unsigned long)(now - loopStartMs_), totalFrameUs_ / 1000.0f / frameNumber_, slowestFrameUs_ / 1000.0f);
      loopStartMs_ = now;
      frameNumber_ = 0;
      slowestFrameUs_ = totalFrameUs_ = 0;
      gif_->reset();
    }
  }

  // The GIF decoder hands over one finished line at a time
  static void copyGifLine(GIFDRAW* line) {
    auto* canvas = static_cast<LGFX_Sprite*>(line->pUser);
    canvas->pushImage(line->iX, line->iY + line->y, line->iWidth, 1, reinterpret_cast<lgfx::swap565_t*>(line->pPixels));
  }

  static void showError(LGFX_Sprite& canvas, const char* text) {
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(2);
    canvas.drawString(text, 10, 10);
  }

  static constexpr const char* FOLDER = "/themes";
  std::vector<String> paths_;

  std::unique_ptr<AnimatedGIF> gif_;  // only exists while a GIF is on screen
  uint8_t* gifFile_ = nullptr;
  uint8_t* gifFrameBuffer_ = nullptr;
  uint32_t frameShownMs_ = 0;
  uint32_t frameDelayMs_ = 100;
  int frameNumber_ = 0;
  uint32_t loopStartMs_ = 0;
  uint32_t totalFrameUs_ = 0, slowestFrameUs_ = 0;
};
