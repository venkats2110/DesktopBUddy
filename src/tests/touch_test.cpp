// Phase 3, step 1 test: head touch pad on GPIO 14 (no display needed).
//
// Prints the touch reading 10 times a second with a bar graph.
// 1. Upload, press RST, open the Serial Monitor.
// 2. Leave the pad alone: the % should sit near 0 (within about +/-1%).
// 3. Touch, tap and stroke the pad: watch how high the % goes.
// Type r + Enter to re-learn the untouched value.
//
// Build it with the "touch_test" environment (see platformio.ini).

#include <Arduino.h>
#include "../pins.h"

float base = 1;
float peak = 0;

void learnBaseline() {
  Serial.println("Learning the untouched value - hands off the pad...");
  uint64_t sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += touchRead(pins::TOUCH_HEAD);
    delay(50);
  }
  base = max(1.0f, sum / 20.0f);
  peak = 0;
  Serial.printf("Untouched value: %.0f. Now touch the pad.\n", base);
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // time for the Serial Monitor to connect after RST
  Serial.printf("\nTouch test - pad on GPIO %d\n", pins::TOUCH_HEAD);
  learnBaseline();
}

void loop() {
  while (Serial.available()) {
    if (Serial.read() == 'r') learnBaseline();
  }

  uint32_t raw = touchRead(pins::TOUCH_HEAD);
  float pct = (raw - base) * 100.0f / base;
  if (pct > peak) peak = pct;

  char bar[41];
  int n = constrain((int)(pct / 2.5f), 0, 40);  // one # per 2.5 %
  memset(bar, '#', n);
  bar[n] = 0;

  Serial.printf("raw %6lu  base %6.0f  rise %+6.1f%%  peak %+6.1f%%  |%s\n",
                (unsigned long)raw, base, pct, peak, bar);
  delay(100);
}
