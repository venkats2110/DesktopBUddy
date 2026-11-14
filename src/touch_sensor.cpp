#include "touch_sensor.h"

namespace touch {
namespace {

// ---- Tuning -----------------------------------------------------------------
// A touch counts when the reading rises this many % above the untouched value,
// and ends when it falls back below TOUCH_OFF_PCT. Set TOUCH_ON_PCT to about half
// the rise you see in touch_test for a light touch through the pad's cover.
constexpr float TOUCH_ON_PCT = 10.0f;  // measured 15 Nov: idle <= 2 %, light tap 17-70 %, press 500 %+
constexpr float TOUCH_OFF_PCT = 5.0f;
// A touch also ends when the reading falls to this fraction of its own peak, so a
// hand hovering just above the pad between taps or strokes doesn't merge them.
constexpr float RELEASE_FRACTION = 0.25f;

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
float level = 0;     // latest reading
float peak = 0;      // highest rise during the current touch
float valley = 0;    // lowest rise since the last release
bool isTouched = false;
bool debugLog = false;

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
  valley = 0;
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

  // No smoothing: the readings are steady, and smoothing made quick taps blur together.
  level = touchRead(padPin);
  float r = rise();

  bool wasTouched = isTouched;
  if (!isTouched) {
    // A new touch must rise clearly above the lowest point since the last one
    // (at least double it, and by TOUCH_ON_PCT), so a hovering hand doesn't count.
    if (r < valley) valley = r;
    float floor = max(valley, 0.0f);
    if (r > floor + max(TOUCH_ON_PCT, floor)) {
      isTouched = true;
      peak = r;
    }
  } else {
    if (r > peak) peak = r;
    if (r < TOUCH_OFF_PCT || r < peak * RELEASE_FRACTION) {
      isTouched = false;
      valley = r;
    }
  }

  // Follow slow drift (temperature, humidity, USB vs battery) only while the pad is
  // idle; a hovering hand must not drag the baseline up.
  if (!isTouched && r < TOUCH_OFF_PCT) {
    base += (level - base) * 0.01f;
    if (r < 0) valley = 0;
  }

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
    if (debugLog) {
      const char* kind = longPressFired         ? "long press"
                         : held < TAP_MAX_MS    ? "tap"
                         : held <= STROKE_MAX_MS ? "stroke"
                                                 : "ignored (2.5-3 s)";
      Serial.printf("  touch up: %lu ms, peak %+.0f%% -> %s\n", (unsigned long)held, peak, kind);
    }
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

void setDebug(bool on) { debugLog = on; }

bool touched() { return isTouched; }
uint32_t raw() { return (uint32_t)level; }
uint32_t baseline() { return (uint32_t)base; }
float risePct() { return rise(); }

}  // namespace touch
