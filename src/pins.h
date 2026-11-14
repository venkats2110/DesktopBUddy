// Deskbot pin map for everything that is not the display (display pins live in
// display_config.h). Keep this in sync with the pin map in the project doc.
//
// Rules: no Wi-Fi/Bluetooth, never GPIO 35/36/37 (octal PSRAM), avoid the
// strapping pins 0/3/45/46.
#pragma once
#include <Arduino.h>

namespace pins {

// Phase 3, step 1: head touch pad (copper tape) on a touch-capable pin.
constexpr uint8_t TOUCH_HEAD = 14;

// Phase 3, step 2: I2C bus shared by the GY-87 and the three VL53L0X sensors.
constexpr uint8_t I2C_SDA = 8;
constexpr uint8_t I2C_SCL = 9;

// Phase 3, step 3: XSHUT lines so the VL53L0X sensors can be given their own
// I2C addresses at boot. The front sensor needs no XSHUT pin: it is the last
// one left at the default address.
constexpr uint8_t XSHUT_CLIFF_LEFT = 15;
constexpr uint8_t XSHUT_CLIFF_RIGHT = 21;

// Arm servo used in the Phase 1 servo test.
constexpr uint8_t SERVO_LEFT = 4;

}  // namespace pins
