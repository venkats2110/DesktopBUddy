// Deskbot - Phase 3: senses
// Board  : 7Semi ESP32-S3-WROOM-1U-N16R8
// Display: DFRobot DFR0664 2.0" 320x240 ST7789 (SPI)
//
// Step 1 (touch): the head pad on GPIO 14 drives the face.
//   pat              -> happy
//   rapid taps       -> grumpy
//   long petting     -> love
//   3-second press   -> asleep (a pat wakes it up again)
//
// Serial Monitor (115200):
//   a face name (e.g. "grumpy") shows that face for a few seconds
//   list  - all face names
//   touch - turn touch logging on/off: one line per touch (for tuning)
//   raw   - turn live touch readings on/off (5 per second)
//   cal   - re-learn the untouched value of the pad (hands off!)

#include <Arduino.h>
#include "display_config.h"
#include "face_engine.h"
#include "pins.h"
#include "touch_sensor.h"

LGFX tft;

constexpr uint32_t HOLD_FOREVER = 0;

int neutralFace = 0;
uint32_t faceUntil = 0;   // when to go back to neutral (0 = stay)
bool faceHeld = false;    // a reaction face is on screen
bool showTouch = false;   // print live touch readings
bool logTouches = false;  // print one line per touch

// Show a face by name and keep it for holdMs, then drift back to neutral.
void react(const char* faceName, uint32_t holdMs) {
  int i = face::find(faceName);
  if (i < 0) {
    Serial.printf("No face called '%s' in faces.h\n", faceName);
    return;
  }
  if (face::current() != i) face::show(i);
  faceHeld = true;
  faceUntil = holdMs == HOLD_FOREVER ? 0 : millis() + holdMs;
}

void listFaces() {
  Serial.print("Faces:");
  for (int i = 0; i < FACE_COUNT; i++) Serial.printf(" %s", FACES[i].name);
  Serial.println();
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch != '\n' && ch != '\r') {
      line += ch;
      continue;
    }
    line.trim();
    if (line == "list") {
      listFaces();
    } else if (line == "touch") {
      logTouches = !logTouches;
      touch::setDebug(logTouches);
      Serial.printf("Touch logging %s\n", logTouches ? "on" : "off");
    } else if (line == "raw") {
      showTouch = !showTouch;
    } else if (line == "cal") {
      Serial.println("Re-learning the touch pad - hands off...");
      touch::recalibrate();
      Serial.printf("Untouched value: %lu\n", (unsigned long)touch::baseline());
    } else if (line.length()) {
      if (face::find(line.c_str()) >= 0) {
        react(line.c_str(), 4000);
        Serial.printf("Face: %s\n", line.c_str());
      } else {
        Serial.printf("Unknown command '%s'. ", line.c_str());
        listFaces();
      }
    }
    line = "";
  }
}

void handleTouch() {
  touch::Gesture g = touch::update();
  if (g != touch::Gesture::None) Serial.printf("Touch: %s\n", touch::name(g));

  switch (g) {
    case touch::Gesture::Pat:       react("happy", 3000); break;
    case touch::Gesture::RapidTaps: react("grumpy", 3000); break;
    case touch::Gesture::Petting:   react("love", 4000); break;
    case touch::Gesture::LongPress: react("asleep", HOLD_FOREVER); break;
    default: break;
  }

  static uint32_t nextPrint = 0;
  if (showTouch && millis() >= nextPrint) {
    nextPrint = millis() + 200;
    Serial.printf("touch raw %lu  base %lu  rise %+.1f%%  %s\n",
                  (unsigned long)touch::raw(), (unsigned long)touch::baseline(),
                  touch::risePct(), touch::touched() ? "TOUCHED" : "");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  tft.init();
  tft.setRotation(1);       // landscape 320x240; use 3 if the face is upside down
  tft.setBrightness(200);   // 0-255
  tft.fillScreen(TFT_BLACK);

  if (!face::begin(tft)) {
    Serial.println("Frame buffer allocation failed - check the PSRAM build flags");
    tft.fillScreen(TFT_RED);
    while (true) delay(1000);
  }

  neutralFace = max(0, face::find("neutral"));
  face::show(neutralFace);

  touch::begin(pins::TOUCH_HEAD);  // hands off the pad while this runs

  Serial.printf("Deskbot Phase 3: %d faces, head pad on GPIO %d (untouched value %lu).\n",
                FACE_COUNT, pins::TOUCH_HEAD, (unsigned long)touch::baseline());
  Serial.println("Type a face name, 'list', 'touch', 'raw' or 'cal'.");
}

void loop() {
  handleSerial();
  handleTouch();

  if (faceHeld && faceUntil != 0 && (int32_t)(millis() - faceUntil) >= 0) {
    faceHeld = false;
    face::show(neutralFace);
  }

  face::update();
}
