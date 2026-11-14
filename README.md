# DesktopBuddy (Deskbot)

A DIY desktop companion robot: a boxy wheeled bot with a screen face, arms, and sleep / active / recharge modes.

## Hardware

| Part | Model |
|---|---|
| MCU | 7Semi ESP32-S3-WROOM-1U-N16R8 (16 MB flash, 8 MB octal PSRAM) |
| Display | DFRobot DFR0664 2.0" 320x240 ST7789 (SPI) |

### Display pin map

| Signal | GPIO |
|---|---|
| SCLK | 12 |
| MOSI | 11 |
| CS | 10 |
| DC | 17 |
| RST | 18 |
| Backlight (PWM) | 16 |

### Rules

- Never enable Wi-Fi or Bluetooth (the -1U module has no antenna fitted).
- Never use GPIO 35/36/37 (used by the octal PSRAM).

## Build and upload

1. Open this folder in VS Code with the PlatformIO extension.
2. Check `upload_port` / `monitor_port` in `platformio.ini` match your machine.
3. Run **PlatformIO: Upload**, then **Monitor** (115200 baud).

## Status

- **Phase 1:** board bring-up ✅
- **Phase 2, step 1:** display bring-up test (`src/main.cpp`): colour flash → "Hello, Venkat!" → blinking, glancing eyes.

### Display troubleshooting

- **Colours wrong/inverted:** flip `cfg.invert` in the panel config.
- **Red and blue swapped:** flip `cfg.rgb_order`.
- **Noise or garbage on screen:** lower `SPI_FREQ` (breadboard wiring rarely handles 40 MHz).
- **Screen stays red after text test:** sprite allocation failed, so check the PSRAM build flags.
