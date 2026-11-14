// Head touch pad: a piece of copper tape on an ESP32-S3 touch pin.
// Turns the raw touch readings into gestures the face can react to.
//
// On the ESP32-S3 the reading goes UP when a finger is on the pad.
// Run the touch_test environment first to see your own numbers, then tune
// TOUCH_ON_PCT / TOUCH_OFF_PCT at the top of touch_sensor.cpp.
#pragma once

#include <Arduino.h>

namespace touch {

enum class Gesture : uint8_t {
  None,
  Pat,        // 1-3 short taps, or 1-2 slow strokes
  RapidTaps,  // 4 or more quick taps in a row (repeats for every extra tap)
  Petting,    // 3 or more slow strokes in a row (repeats for every extra stroke)
  LongPress,  // finger held on the pad for 3 seconds
};

const char* name(Gesture g);

// Learns the untouched value of the pad. Keep hands off the pad while it runs (~0.5 s).
void begin(uint8_t pin);

// Call every loop. Returns a gesture once, at the moment it is recognised.
Gesture update();

// Re-learn the untouched value (e.g. after moving from USB to battery power).
void recalibrate();

// Print one line per touch (how long it lasted, its peak and what it counted as).
void setDebug(bool on);

bool touched();       // true while a finger is on the pad
uint32_t raw();       // latest smoothed reading
uint32_t baseline();  // learned untouched value
float risePct();      // how far the reading is above the baseline, in %

}  // namespace touch
