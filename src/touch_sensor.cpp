#include "touch_sensor.h"

namespace touch {
namespace {

// ---- Tuning -----------------------------------------------------------------
// A touch counts when the reading rises this many % above the untouched value,
// and ends when it falls back below TOUCH_OFF_PCT. Set TOUCH_ON_PCT to about half
// the rise you see in touch_test for a light touch through the pad's cover.
constexpr float TOUCH_ON_PCT = 6.0f;
constexpr float TOUCH_OFF_PCT = 3.5f;

constexpr uint32_t SAMPLE_MS = 20;        // 50 readings a second
constexpr uint32_t TAP_MAX_MS = 300;      // a touch shorter than this is a tap
constexpr uint32_t STROKE_MAX_MS = 2500;  // 300-2500 ms is a stroke (petting)
constexpr uint32_t LONG_PRESS_MS = 3000;  // held this long = long press
constexpr uint32_t TAP_GAP_MS = 700;      // quiet time that ends a run of taps
constexpr uint32_t STROKE_GAP_MS = 1500;  // quiet time that ends a run of strokes
constexpr uint32_t STUCK_MS = 20000;      // "touched" this long = drifted, re-learn
constexpr int RAPID_TAPS = 4;
constexpr int PETTING_STROKES = 3;
// -----------------------------------------------------------------------------

uint8_t padPin = 0;
float base = 1;      // untouched value
float level = 0;     // smoothed reading
bool isTouched = false;

uint32_t lastSample = 0;
uint32_t pressStart = 0;
uint32_t lastRelease = 0;
uint32_t quietMs = STROKE_GAP_MS;
bool longPressFired = false;

// The current run of touches ("sequence"); it ends after a quiet gap.
bool inSequence = false;
bool reported = false;  // a gesture was already sent for this sequence
int taps = 0;
int strokes = 0;

float rise() { return (level - base) * 100.0f / base; }

void resetSequence() {
  inSequence = false;
  reported = false;
  taps = 0;
  strokes = 0;
}

}  // namespace

const char* name(Gesture g) {
  switch (g) {
    case Gesture::Pat: return "pat";
    case Gesture::RapidTaps: return "rapid taps";
    case Gesture::Petting: return "petting";
    case Gesture::LongPress: return "long press";
    default: return "none";
  }
}

void recalibrate() {
  uint64_t sum = 0;
  for (int i = 0; i < 25; i++) {
    sum += touchRead(padPin);
    delay(20);
  }
  base = max(1.0f, sum / 25.0f);
  level = base;
  isTouched = false;
  resetSequence();
}

void begin(uint8_t pin) {
  padPin = pin;
  recalibrate();
}

Gesture update() {
  uint32_t now = millis();
  if (now - lastSample < SAMPLE_MS) return Gesture::None;
  lastSample = now;

  level += (touchRead(padPin) - level) * 0.5f;  // light smoothing
  float r = rise();

  bool wasTouched = isTouched;
  if (!isTouched && r > TOUCH_ON_PCT) isTouched = true;
  else if (isTouched && r < TOUCH_OFF_PCT) isTouched = false;

  // Follow slow drift (temperature, humidity, USB vs battery) while untouched.
  if (!isTouched) base += (level - base) * 0.01f;

  Gesture g = Gesture::None;

  if (isTouched && !wasTouched) {
    // Finger down.
    pressStart = now;
    longPressFired = false;
    inSequence = true;
  } else if (isTouched) {
    // Finger still down.
    uint32_t held = now - pressStart;
    if (!longPressFired && held >= LONG_PRESS_MS) {
      longPressFired = true;
      reported = true;
      g = Gesture::LongPress;
    } else if (held >= STUCK_MS) {
      recalibrate();  // nobody holds that long: the baseline drifted
    }
  } else if (wasTouched) {
    // Finger lifted: classify the touch by how long it lasted.
    uint32_t held = now - pressStart;
    lastRelease = now;
    quietMs = STROKE_GAP_MS;
    if (!longPressFired) {
      if (held < TAP_MAX_MS) {
        quietMs = TAP_GAP_MS;
        if (++taps >= RAPID_TAPS) g = Gesture::RapidTaps;
      } else if (held <= STROKE_MAX_MS) {
        if (++strokes >= PETTING_STROKES) g = Gesture::Petting;
      }
    }
    if (g != Gesture::None) reported = true;
  } else if (inSequence && now - lastRelease > quietMs) {
    // Quiet long enough: the sequence is over. A short one is a pat.
    if (!reported && (taps > 0 || strokes > 0)) g = Gesture::Pat;
    resetSequence();
  }

  return g;
}

bool touched() { return isTouched; }
uint32_t raw() { return (uint32_t)level; }
uint32_t baseline() { return (uint32_t)base; }
float risePct() { return rise(); }

}  // namespace touch
