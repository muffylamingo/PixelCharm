// Tells LovyanGFX which display we have and which pins it is wired to.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "../board_devkit.h"  // the pins

// platformio.ini picks the panel with a build flag. The Wokwi website has no build flags, so ILI9341 is the default.
#if !defined(PANEL_ILI9341) && !defined(PANEL_ST7789)
#define PANEL_ILI9341
#endif

#ifdef PANEL_ILI9341

// ---------- Wokwi: 2.8" ILI9341, 240x320 ----------

// Wokwi's ILI9341 is only the right way round when the MADCTL (memory direction) register has the MX bit.
// With the BGR colour-order bit LovyanGFX adds, that is 0x48, the same value the Adafruit library sends.
// LovyanGFX's default (0x08) shows the picture mirrored left-to-right there.
struct Panel_ILI9341_Wokwi : public lgfx::Panel_ILI9341 {
 protected:
  uint8_t getMadCtl(uint8_t rotation) const override {
    if (rotation == 0) return MAD_MX;
    return lgfx::Panel_ILI9341::getMadCtl(rotation);
  }
};

class LGFX : public lgfx::LGFX_Device {
  Panel_ILI9341_Wokwi panel_;
  lgfx::Bus_SPI bus_;

 public:
  LGFX() {
    auto busCfg = bus_.config();
    busCfg.spi_host = SPI2_HOST;
    busCfg.spi_mode = 0;
    busCfg.freq_write = 40000000;  // 40 MHz
    // No DMA in Wokwi: a full-screen sprite sent with DMA came out as stripes there, without DMA it was correct.
    busCfg.dma_channel = 0;
    busCfg.pin_sclk = PIN_SCK;
    busCfg.pin_mosi = PIN_MOSI;
    busCfg.pin_miso = -1;  // we never read from the display
    busCfg.pin_dc = PIN_DC;
    bus_.config(busCfg);
    panel_.setBus(&bus_);

    auto panelCfg = panel_.config();
    panelCfg.pin_cs = PIN_CS;
    panelCfg.pin_rst = PIN_RST;
    panelCfg.pin_busy = -1;
    panelCfg.panel_width = 240;
    panelCfg.panel_height = 320;
    panel_.config(panelCfg);

    setPanel(&panel_);
  }
};

#else

// ---------- Real board: 1.3" ST7789, 240x240, 7 pins (no CS) ----------

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM backlight_;

 public:
  LGFX() {
    auto busCfg = bus_.config();
    busCfg.spi_host = SPI2_HOST;
    busCfg.spi_mode = 3;  // ST7789 screens without a CS pin only listen in SPI mode 3
    busCfg.freq_write = 40000000;  // 40 MHz
    busCfg.dma_channel = SPI_DMA_CH_AUTO;  // frees the CPU while a frame is sent (off in Wokwi, see above)
    busCfg.pin_sclk = PIN_SCK;
    busCfg.pin_mosi = PIN_MOSI;
    busCfg.pin_miso = -1;  // we never read from the display
    busCfg.pin_dc = PIN_DC;
    bus_.config(busCfg);
    panel_.setBus(&bus_);

    auto panelCfg = panel_.config();
    panelCfg.pin_cs = -1;  // no CS pin: the screen is the only device on this SPI bus
    panelCfg.pin_rst = PIN_RST;
    panelCfg.pin_busy = -1;
    panelCfg.panel_width = 240;
    panelCfg.panel_height = 240;
    panelCfg.memory_height = 320;  // the ST7789 chip has memory for 240x320; our glass shows 240x240 of it
    panelCfg.invert = true;  // IPS ST7789 screens show negative colours without this
    panel_.config(panelCfg);

    // The backlight is dimmed by switching it on and off very fast (PWM)
    auto lightCfg = backlight_.config();
    lightCfg.pin_bl = PIN_BACKLIGHT;
    lightCfg.freq = 44100;  // fast enough that the eye cannot see it flicker
    lightCfg.pwm_channel = 7;
    backlight_.config(lightCfg);
    panel_.setLight(&backlight_);

    setPanel(&panel_);
  }
};

#endif
