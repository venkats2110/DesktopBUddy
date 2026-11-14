// Face engine: draws the expressions from include/faces.h and animates them.
// Tweening between faces, auto-blink, idle glances and breathing all live here,
// so the rest of the firmware only has to say which face to show.
#pragma once

#include <Arduino.h>
#include "display_config.h"
#include "faces.h"

namespace face {

// Allocate the 320x240 frame buffer in PSRAM. Returns false if that fails.
bool begin(LGFX& lcd);

// Start a smooth transition to FACES[index].
void show(int index);

// Index of the face with this name (case-insensitive), or -1.
int find(const char* name);

// Index of the face currently shown (or being tweened to).
int current();

// Draw one frame. Call this every loop.
void update();

}  // namespace face
