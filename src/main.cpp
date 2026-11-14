// Deskbot - Phase 2: face engine
// Board  : 7Semi ESP32-S3-WROOM-1U-N16R8
// Display: DFRobot DFR0664 2.0" 320x240 ST7789 (SPI)
//
// Shows every expression from include/faces.h (exported from the Face Lab),
// cycling every 2.5 s with smooth transitions, blinking, glances and breathing.
//
// Serial Monitor (115200): type a face name (e.g. "grumpy") and press Enter to
// hold that face. Type "auto" to go back to cycling, "list" to see all names.

#include <Arduino.h>
#include "display_config.h"
#include "face_engine.h"

LGFX tft;

constexpr uint32_t CYCLE_MS = 2500;
bool autoCycle = true;
uint32_t nextCycle = 0;

void listFaces() {
  Serial.print("Faces:");
  for (int i = 0; i < FACE_COUNT; i++) Serial.printf(" %s", FACES[i].name);
  Serial.println(" (or auto)");
}

void showFace(int index) {
  face::show(index);
  Serial.printf("Face: %s\n", FACES[index].name);
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
    if (line == "auto") {
      autoCycle = true;
      nextCycle = millis();
      Serial.println("Cycling through all faces");
    } else if (line == "list") {
      listFaces();
    } else if (line.length()) {
      int i = face::find(line.c_str());
      if (i >= 0) {
        autoCycle = false;
        showFace(i);
      } else {
        Serial.printf("Unknown face '%s'. ", line.c_str());
        listFaces();
      }
    }
    line = "";
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

  Serial.printf("Deskbot face engine: %d faces loaded. Type a face name, 'list' or 'auto'.\n",
                FACE_COUNT);
  showFace(0);
  nextCycle = millis() + CYCLE_MS;
}

void loop() {
  handleSerial();
  if (autoCycle && millis() >= nextCycle) {
    showFace((face::current() + 1) % FACE_COUNT);
    nextCycle = millis() + CYCLE_MS;
  }
  face::update();
}
