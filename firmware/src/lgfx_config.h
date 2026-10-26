// Tells LovyanGFX which display we have and which pins it is wired to.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Pin plan from PLAN.md: the same pins are used in Wokwi and on the real board.
constexpr int PIN_SCK = 12;
constexpr int PIN_MOSI = 11;
constexpr int PIN_CS = 10;
constexpr int PIN_DC = 9;
constexpr int PIN_RST = 14;

// platformio.ini picks the panel with a build flag. The Wokwi website has no build flags, so ILI9341 is the default.
#if !defined(PANEL_ILI9341) && !defined(PANEL_ST7789)
#define PANEL_ILI9341
#endif

#ifndef PANEL_ILI9341
#error "Only the Wokwi display (ILI9341) is configured so far. The real ST7789 comes in task 1.7."
#endif

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
    busCfg.dma_channel = SPI_DMA_CH_AUTO;
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
