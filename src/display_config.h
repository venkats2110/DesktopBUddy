// Display driver setup for the DFRobot DFR0664 2.0" 320x240 ST7789 (SPI).
// Settings here are the ones confirmed working in the Phase 2 bring-up test.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ---------- Pin map ----------
constexpr int PIN_SCLK = 12;
constexpr int PIN_MOSI = 11;
constexpr int PIN_CS   = 10;
constexpr int PIN_DC   = 17;
constexpr int PIN_RST  = 18;
constexpr int PIN_BL   = 16;

// Breadboard jumpers rarely handle 40 MHz. 27 MHz is safe; try 40 MHz once soldered.
constexpr uint32_t SPI_FREQ = 27000000;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

 public:
  LGFX() {
    {  // SPI bus
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = SPI_FREQ;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;  // no MISO wired
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_SCLK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = PIN_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {  // Panel
      auto cfg = _panel.config();
      cfg.pin_cs          = PIN_CS;
      cfg.pin_rst         = PIN_RST;
      cfg.pin_busy        = -1;
      cfg.memory_width    = 240;
      cfg.memory_height   = 320;
      cfg.panel_width     = 240;
      cfg.panel_height    = 320;
      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = false;
      cfg.invert          = true;   // IPS ST7789 panels need this
      cfg.rgb_order       = false;  // set true if red and blue are swapped
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      _panel.config(cfg);
    }
    {  // Backlight on PWM so Sleep mode can dim it later
      auto cfg = _light.config();
      cfg.pin_bl      = PIN_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};
