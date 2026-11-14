// Deskbot - Phase 2, step 1: display bring-up test
// Board  : 7Semi ESP32-S3-WROOM-1U-N16R8
// Display: DFRobot DFR0664 2.0" 320x240 ST7789 (SPI)
//
// What you should see:
//   1. Screen flashes RED -> GREEN -> BLUE -> WHITE (1 s each)
//   2. "Hello, Venkat!" text
//   3. Two rounded eyes that blink every few seconds and glance left/right
//
// Put this file at src/main.cpp in your PlatformIO project (replace the old blink test).

#include <Arduino.h>
#include <LovyanGFX.hpp>

// ---------- Pin map (matches the build notes) ----------
#define PIN_SCLK 12
#define PIN_MOSI 11
#define PIN_CS   10
#define PIN_DC   17
#define PIN_RST  18
#define PIN_BL   16

// On a breadboard with jumper wires, 40 MHz can give garbage/noise.
// Start at 27 MHz. Once soldered with short wires, try 40 MHz.
#define SPI_FREQ 27000000

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX() {
    { // SPI bus
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = SPI_FREQ;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;      // no MISO wired
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = PIN_SCLK;
      cfg.pin_mosi    = PIN_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = PIN_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    { // Panel
      auto cfg = _panel.config();
      cfg.pin_cs        = PIN_CS;
      cfg.pin_rst       = PIN_RST;
      cfg.pin_busy      = -1;
      cfg.memory_width  = 240;
      cfg.memory_height = 320;
      cfg.panel_width   = 240;
      cfg.panel_height  = 320;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.readable      = false;
      cfg.invert        = true;    // IPS ST7789 panels need this, else colours look inverted
      cfg.rgb_order     = false;
      cfg.dlen_16bit    = false;
      cfg.bus_shared    = false;
      _panel.config(cfg);
    }
    { // Backlight on GPIO 16 (PWM so we can dim it later for Sleep mode)
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

LGFX tft;
LGFX_Sprite canvas(&tft);   // full-screen buffer in PSRAM -> flicker-free animation

const uint16_t BG  = TFT_BLACK;
const uint16_t EYE = 0x5EFF;   // soft cyan

// Eye state
float lookX = 0;          // -1 (left) .. +1 (right)
float targetX = 0;
float openness = 1.0;     // 1 = open, 0 = closed
uint32_t nextBlink = 0, nextGlance = 0;

void drawEyes() {
  canvas.fillScreen(BG);
  const int eyeW = 70, eyeH = 90, gap = 60;
  const int cx = 160 + (int)(lookX * 25);
  const int cy = 120;
  int h = max(6, (int)(eyeH * openness));
  canvas.fillRoundRect(cx - gap/2 - eyeW, cy - h/2, eyeW, h, 18, EYE);
  canvas.fillRoundRect(cx + gap/2,        cy - h/2, eyeW, h, 18, EYE);
  canvas.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Deskbot display test starting...");

  tft.init();
  tft.setRotation(1);       // landscape, 320 x 240
  tft.setBrightness(200);   // 0-255

  // 1. Colour test: if these colours are wrong, see troubleshooting notes
  uint16_t colours[] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE };
  const char* names[] = { "RED", "GREEN", "BLUE", "WHITE" };
  for (int i = 0; i < 4; i++) {
    tft.fillScreen(colours[i]);
    Serial.printf("Showing %s\n", names[i]);
    delay(1000);
  }

  // 2. Text test
  tft.fillScreen(BG);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(middle_center);
  tft.setFont(&fonts::FreeSansBold18pt7b);
  tft.drawString("Hello, Venkat!", 160, 120);
  delay(2000);

  // 3. Sprite in PSRAM for the eyes
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(320, 240)) {
    Serial.println("Sprite alloc FAILED - check PSRAM build flags");
    tft.fillScreen(TFT_RED);
    while (true) delay(1000);
  }
  Serial.println("Eyes running.");
  nextBlink = millis() + 2000;
  nextGlance = millis() + 3000;
}

void loop() {
  uint32_t now = millis();

  // Blink: close and open over ~200 ms
  if (now >= nextBlink) {
    for (float o = 1.0; o > 0.05; o -= 0.2) { openness = o; drawEyes(); delay(16); }
    for (float o = 0.05; o <= 1.0; o += 0.2) { openness = o; drawEyes(); delay(16); }
    openness = 1.0;
    nextBlink = now + random(2500, 6000);
  }

  // Glance somewhere new every few seconds
  if (now >= nextGlance) {
    int r = random(3);
    targetX = (r == 0) ? -1 : (r == 1) ? 1 : 0;
    nextGlance = now + random(2000, 5000);
  }

  lookX += (targetX - lookX) * 0.15f;   // smooth easing
  drawEyes();
  delay(16);                             // ~60 fps
}