// The picture themes: 240x240 JPEGs and animated GIFs on the board's LittleFS "disk".
// tools/prepare_images.py makes them (and the 64x64 menu thumbnails); "Upload Filesystem Image" copies them over.
#pragma once
#include <AnimatedGIF.h>
#include <JPEGDEC.h>
#include <LittleFS.h>

#include <memory>

#include "theme.h"

namespace picture {

// Reads a whole file into RAM. `psram` = the big, slower 8 MB RAM. Returns nullptr on failure; free() it after.
inline uint8_t* readFile(const String& path, size_t& size, bool psram) {
  File file = LittleFS.open(path);
  if (!file) return nullptr;
  size = file.size();
  auto* data = static_cast<uint8_t*>(psram ? ps_malloc(size) : malloc(size));
  if (data && file.read(data, size) != size) {
    free(data);
    data = nullptr;
  }
  return data;
}

// The JPEG decoder hands over the picture a few blocks at a time; each block goes into its place in the canvas
inline int copyJpegBlock(JPEGDRAW* block) {
  auto* canvas = static_cast<LGFX_Sprite*>(block->pUser);
  canvas->pushImage(block->x, block->y, block->iWidth, block->iHeight, reinterpret_cast<lgfx::swap565_t*>(block->pPixels));
  return 1;  // 1 = keep going
}

// Draws a JPEG file into the canvas with its top left at (x, y). Can report how long reading and decoding took.
inline bool drawJpegFile(LGFX_Sprite& canvas, const String& path, int x, int y, uint32_t* readUs = nullptr,
                         uint32_t* decodeUs = nullptr) {
  uint32_t startUs = micros();
  size_t size = 0;
  std::unique_ptr<uint8_t, decltype(&free)> jpeg(readFile(path, size, false), &free);
  uint32_t readDoneUs = micros();
  // The decoder needs ~17 KB of working memory: borrow it only while decoding
  std::unique_ptr<JPEGDEC> decoder(new JPEGDEC);
  bool ok = jpeg && decoder->openRAM(jpeg.get(), size, copyJpegBlock);
  if (ok) {
    decoder->setUserPointer(&canvas);
    // Big-endian RGB565 is exactly how the canvas stores its pixels, so blocks can be copied as they are
    decoder->setPixelType(RGB565_BIG_ENDIAN);
    ok = decoder->decode(x, y, 0) == 1;
    decoder->close();
  }
  if (readUs) *readUs = readDoneUs - startUs;
  if (decodeUs) *decodeUs = micros() - readDoneUs;
  return ok;
}

inline void showError(LGFX_Sprite& canvas, const char* text) {
  canvas.setTextColor(TFT_RED);
  canvas.setTextSize(2);
  canvas.drawString(text, 10, 10);
}

}  // namespace picture

// What still pictures and GIFs share: a name, a file and a thumbnail for the menu
class PictureTheme : public Theme {
 public:
  PictureTheme(String name, String file, String thumb) : name_(name), file_(file), thumb_(thumb) {}
  String id() const override { return file_; }
  String name() const override { return name_; }
  void drawIcon(LGFX_Sprite& canvas, int x, int y, int size) const override {
    if (!picture::drawJpegFile(canvas, thumb_, x, y)) canvas.fillRect(x, y, size, size, TFT_DARKGREY);
  }

 protected:
  String name_, file_, thumb_;
};

// A JPEG. It doesn't move, so it is decoded once when it comes on screen and then stays in the canvas.
class ImageTheme : public PictureTheme {
 public:
  using PictureTheme::PictureTheme;

  void enter(LGFX_Sprite& canvas, uint32_t now) override {
    canvas.fillScreen(TFT_BLACK);
    uint32_t readUs = 0, decodeUs = 0;
    bool ok = picture::drawJpegFile(canvas, file_, 0, 0, &readUs, &decodeUs);
    Serial.printf("JPEG %s: %s, okuma %.1f ms + cozme %.1f ms\n", file_.c_str(), ok ? "cizildi" : "HATA", readUs / 1000.0f,
                  decodeUs / 1000.0f);
    if (!ok) picture::showError(canvas, "JPEG okunamadi");
  }
  void frame(LGFX_Sprite&, uint32_t, float) override {}  // nothing to do: the picture is already in the canvas
};

// An animated GIF. The file goes into PSRAM when the theme comes on screen, and each frame is decoded when it is due.
class GifTheme : public PictureTheme {
 public:
  using PictureTheme::PictureTheme;
  ~GifTheme() override { leave(); }

  void enter(LGFX_Sprite& canvas, uint32_t now) override {
    canvas.fillScreen(TFT_BLACK);
    uint32_t startUs = micros();
    size_t size = 0;
    // A GIF can be 1 MB: it goes into PSRAM, and frames are decoded from there
    file_data_ = picture::readFile(file_, size, true);
    uint32_t readUs = micros() - startUs;
    gif_.reset(new AnimatedGIF);
    gif_->begin(GIF_PALETTE_RGB565_BE);  // the canvas's own pixel format
    if (!file_data_ || !gif_->open(file_data_, size, copyGifLine)) {
      Serial.printf("GIF %s acilamadi\n", file_.c_str());
      leave();
      picture::showError(canvas, "GIF okunamadi");
      return;
    }
    // "Cooked" mode: the library keeps the whole picture as 8-bit colour numbers (a frame may change only a part
    // of it) and hands us finished RGB565 lines. It needs width x (height + 2) bytes for that.
    int width = gif_->getCanvasWidth(), height = gif_->getCanvasHeight();
    frame_buffer_ = static_cast<uint8_t*>(malloc(width * (height + 2)));
    if (!frame_buffer_) {
      Serial.println("GIF icin RAM yetmedi");
      leave();
      picture::showError(canvas, "RAM yetmedi");
      return;
    }
    gif_->setDrawType(GIF_DRAW_COOKED);
    gif_->setFrameBuf(frame_buffer_);
    GIFINFO info;
    gif_->getInfo(&info);
    Serial.printf("GIF %s: %ux%u, %u KB, %d kare, %.2f sn, okuma %.1f ms. Kalan RAM %u KB, PSRAM %u KB\n", file_.c_str(), width,
                  height, (unsigned)(size / 1024), (int)info.iFrameCount, info.iDuration / 1000.0f, readUs / 1000.0f,
                  (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
    frameNumber_ = 0;
    slowestFrameUs_ = totalFrameUs_ = 0;
    loopStartMs_ = now;
    nextFrame(canvas);
    frameDueMs_ = millis();
  }

  void frame(LGFX_Sprite& canvas, uint32_t now, float) override {
    if (!gif_ || now - frameDueMs_ < frameDelayMs_) return;
    // Keep to the GIF's own timing: the next frame is due one delay after the previous one was due.
    // If we fell far behind, start counting from now instead of rushing to catch up.
    frameDueMs_ += frameDelayMs_;
    if (now - frameDueMs_ > frameDelayMs_) frameDueMs_ = now;
    nextFrame(canvas);
  }

  void leave() override {
    if (gif_) gif_->close();
    gif_.reset();
    free(frame_buffer_);
    free(file_data_);
    frame_buffer_ = file_data_ = nullptr;
  }

 private:
  void nextFrame(LGFX_Sprite& canvas) {
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

  std::unique_ptr<AnimatedGIF> gif_;  // only exists while the GIF is on screen
  uint8_t* file_data_ = nullptr;
  uint8_t* frame_buffer_ = nullptr;
  uint32_t frameDueMs_ = 0;
  uint32_t frameDelayMs_ = 100;
  int frameNumber_ = 0;
  uint32_t loopStartMs_ = 0;
  uint32_t totalFrameUs_ = 0, slowestFrameUs_ = 0;
};
