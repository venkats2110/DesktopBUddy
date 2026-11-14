#include "face_engine.h"

#include <math.h>
#include <strings.h>

namespace face {
namespace {

constexpr int W = 320, H = 240;
constexpr uint16_t BG    = 0x0000;
constexpr uint16_t HEART = 0xFAD1;  // #FF5C8A
constexpr uint16_t DROP  = 0x8E9F;  // #8FD3FF

constexpr uint32_t TWEEN_MS = 320;

LGFX* lcd = nullptr;
LGFX_Sprite spr;

int curIdx = 0;
Face fromFace;
uint16_t fromColor = EYE_COLOR, toColor = EYE_COLOR;
uint32_t tweenStart = 0;

uint32_t blinkStart = 0, nextBlink = 2000, secondBlink = 0;
float gx = 0, gy = 0, gtx = 0, gty = 0;
uint32_t nextGlance = 1500;

// ---------- helpers ----------
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float easeInOut(float t) { return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2; }

uint16_t mix565(uint16_t a, uint16_t b, float t) {
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  int r = ar + (br - ar) * t, g = ag + (bg - ag) * t, bl = ab + (bb - ab) * t;
  return (r << 11) | (g << 5) | bl;
}
uint16_t scale565(uint16_t c, float a) { return mix565(BG, c, clampf(a, 0, 1)); }
uint16_t faceColor(const Face& f) { return f.color ? f.color : EYE_COLOR; }

// Numbers ease between faces; mouth shape and extras switch halfway through.
Face blend(const Face& a, const Face& b, float t) {
  Face o = b;
#define L(x) o.x = lerpf(a.x, b.x, t)
  L(eyeW); L(eyeH); L(eyeGap); L(eyeY); L(radius); L(leftScale); L(rightScale);
  L(lidTop); L(lidAngle); L(lidBottom); L(lookX); L(lookY); L(pupil);
  L(mouthCurve); L(mouthW); L(mouthOpen); L(mouthY);
  L(blushAlpha); L(blushW); L(blushH); L(blushX); L(blushY);
#undef L
  o.mouth = t < 0.5f ? a.mouth : b.mouth;
  o.fx = t < 0.5f ? a.fx : b.fx;
  return o;
}

// ---------- anti-aliased shapes ----------
// Every shape is described by a signed distance: how far a pixel centre is
// outside the edge (negative = inside). Pixels well inside are filled in fast
// horizontal runs; pixels on the edge get a blended colour based on how much
// of the pixel the shape covers. That removes the stair-steps on curves and
// diagonals, and lets shapes sit at fractional positions so motion is smoother.

struct Box { int x0, y0, x1, y1; };  // inclusive pixel bounds
const Box SCREEN = {0, 0, W - 1, H - 1};

inline int imax(int a, int b) { return a > b ? a : b; }
inline int imin(int a, int b) { return a < b ? a : b; }
inline float coverage(float d) { return clampf(0.5f - d, 0, 1); }

void blendPixel(int x, int y, uint16_t c, float a) {
  spr.drawPixel(x, y, a >= 0.996f ? c : mix565(spr.readPixel(x, y), c, a));
}

template <typename Dist>
void fillAA(float bx0, float by0, float bx1, float by1, const Box& clip, uint16_t c, Dist dist) {
  int x0 = (int)floorf(bx0) - 1, y0 = (int)floorf(by0) - 1;
  int x1 = (int)ceilf(bx1) + 1, y1 = (int)ceilf(by1) + 1;
  x0 = imax(x0, imax(clip.x0, 0));
  y0 = imax(y0, imax(clip.y0, 0));
  x1 = imin(x1, imin(clip.x1, W - 1));
  y1 = imin(y1, imin(clip.y1, H - 1));
  for (int y = y0; y <= y1; y++) {
    int run = -1;
    for (int x = x0; x <= x1; x++) {
      float a = coverage(dist(x + 0.5f, y + 0.5f));
      if (a >= 0.996f) {
        if (run < 0) run = x;
        continue;
      }
      if (run >= 0) {
        spr.drawFastHLine(run, y, x - run, c);
        run = -1;
      }
      if (a > 0.004f) blendPixel(x, y, c, a);
    }
    if (run >= 0) spr.drawFastHLine(run, y, x1 - run + 1, c);
  }
}

// Rounded rectangle with top-left corner (x, y).
void aaRoundRect(float x, float y, float w, float h, float r, uint16_t c, const Box& clip = SCREEN) {
  if (w <= 0 || h <= 0) return;
  float hw = w / 2, hh = h / 2, cx = x + hw, cy = y + hh;
  r = clampf(r, 0, fminf(hw, hh));
  fillAA(x, y, x + w, y + h, clip, c, [=](float px, float py) {
    float qx = fabsf(px - cx) - (hw - r), qy = fabsf(py - cy) - (hh - r);
    float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
    return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
  });
}

void aaCircle(float x, float y, float r, uint16_t c, const Box& clip = SCREEN) {
  if (r <= 0) return;
  fillAA(x - r, y - r, x + r, y + r, clip, c, [=](float px, float py) {
    float dx = px - x, dy = py - y;
    return sqrtf(dx * dx + dy * dy) - r;
  });
}

void aaEllipse(float x, float y, float rx, float ry, uint16_t c, const Box& clip = SCREEN) {
  if (rx <= 0 || ry <= 0) return;
  fillAA(x - rx, y - ry, x + rx, y + ry, clip, c, [=](float px, float py) {
    float ux = (px - x) / rx, uy = (py - y) / ry;
    float k0 = sqrtf(ux * ux + uy * uy);
    float vx = ux / rx, vy = uy / ry;
    float k1 = sqrtf(vx * vx + vy * vy);
    return k1 > 1e-6f ? k0 * (k0 - 1) / k1 : -fminf(rx, ry);
  });
}

// Convex polygon (3 or 4 corners, either winding).
void aaPolygon(const float* xs, const float* ys, int n, uint16_t c, const Box& clip = SCREEN) {
  float area = 0, bx0 = xs[0], by0 = ys[0], bx1 = xs[0], by1 = ys[0];
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    area += xs[i] * ys[j] - xs[j] * ys[i];
    bx0 = fminf(bx0, xs[i]); by0 = fminf(by0, ys[i]);
    bx1 = fmaxf(bx1, xs[i]); by1 = fmaxf(by1, ys[i]);
  }
  if (fabsf(area) < 1e-3f) return;
  float s = area > 0 ? 1 : -1;
  float nx[4], ny[4], off[4];
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    float ex = xs[j] - xs[i], ey = ys[j] - ys[i], len = sqrtf(ex * ex + ey * ey);
    if (len < 1e-6f) len = 1e-6f;
    nx[i] = s * ey / len;  // outward normal
    ny[i] = -s * ex / len;
    off[i] = nx[i] * xs[i] + ny[i] * ys[i];
  }
  fillAA(bx0, by0, bx1, by1, clip, c, [&](float px, float py) {
    float d = -1e9f;
    for (int i = 0; i < n; i++) d = fmaxf(d, nx[i] * px + ny[i] * py - off[i]);
    return d;
  });
}

void aaTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
  float xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  aaPolygon(xs, ys, 3, c);
}

void heart(float x, float y, float s, uint16_t c) {
  float r = s * 0.5f;
  aaCircle(x - r * 0.95f, y - r * 0.3f, r, c);
  aaCircle(x + r * 0.95f, y - r * 0.3f, r, c);
  aaTriangle(x - r * 1.92f, y - r * 0.05f, x + r * 1.92f, y - r * 0.05f, x, y + r * 1.95f, c);
}

void drop(float x, float y, float s, uint16_t c) {
  aaCircle(x, y + s * 0.6f, s * 0.6f, c);
  aaTriangle(x - s * 0.55f, y + s * 0.45f, x + s * 0.55f, y + s * 0.45f, x, y - s * 0.7f, c);
}

// A "Z" drawn with three smooth strokes, centred on (x, y).
void letterZ(float x, float y, float h, float stroke, uint16_t c) {
  float w = h * 0.8f, l = x - w / 2, r = x + w / 2, t = y - h / 2, b = y + h / 2;
  spr.drawWideLine(l, t, r, t, stroke, c);
  spr.drawWideLine(r, t, l, b, stroke, c);
  spr.drawWideLine(l, b, r, b, stroke, c);
}

void spiral(float x, float y, float R, float rot, uint16_t c) {
  float px = x, py = y;
  for (float a = 0.15f; a < 6 * PI; a += 0.15f) {
    float r = R * a / (6 * PI);
    float nx = x + cosf(a + rot) * r, ny = y + sinf(a + rot) * r;
    spr.drawWideLine(px, py, nx, ny, 2.0f, c);
    px = nx;
    py = ny;
  }
}

struct Eye { float ex, ey, w, h; int side; };

void drawFace(const Face& p, uint16_t ec, uint32_t t, float blink, float bob) {
  spr.fillScreen(BG);
  float lx = clampf(p.lookX + gx, -1, 1), ly = clampf(p.lookY + gy, -1, 1);
  float sx = lx * 28, sy = ly * 20, cx = W / 2.0f, cy = H / 2.0f + p.eyeY + bob;
  Eye eyes[2];

  for (int i = 0; i < 2; i++) {
    int side = i == 0 ? -1 : 1;
    float sc = side < 0 ? p.leftScale : p.rightScale;
    float w = p.eyeW * sc, h = fmaxf(2, p.eyeH * sc * (1 - blink * 0.92f));
    float ex = cx + side * (p.eyeGap / 2 + p.eyeW / 2) + sx, ey = cy + sy;
    eyes[i] = {ex, ey, w, h, side};

    if (p.fx & FX_HEARTS) {
      heart(ex, ey, fminf(w, p.eyeH * sc) * 0.5f * (1 + 0.07f * sinf(t / 180.0f)), HEART);
      continue;
    }
    if (p.fx & FX_SPIRAL) {
      spiral(ex, ey, fminf(w, p.eyeH * sc) * 0.5f, t / 260.0f * side, ec);
      continue;
    }

    float x0 = ex - w / 2, y0 = ey - h / 2;
    aaRoundRect(x0, y0, w, h, p.radius * sc, ec);
    // Lids only need to touch this eye, so limit their work to its area.
    Box eyeBox = {(int)floorf(x0) - 4, (int)floorf(y0) - 4, (int)ceilf(x0 + w) + 4, (int)ceilf(y0 + h) + 4};

    if (p.pupil > 0 && h > 10) {
      float r = p.pupil * fminf(w, h) * 0.5f;
      float px = ex + lx * w * 0.18f, py = ey + ly * h * 0.18f;
      aaCircle(px, py, r, BG);
      aaCircle(px + r * 0.35f, py - r * 0.35f, fmaxf(1.5f, r * 0.22f), ec);
    }

    // Top lid: a background-coloured quad; the inner corner drops when the angle is positive.
    float dropY = p.lidTop * h, slope = tanf(p.lidAngle * PI / 180) * w / 2;
    if (dropY > 0 || fabsf(slope) > 0) {
      float inner = y0 + dropY + slope, outer = y0 + dropY - slope;
      float yL = side < 0 ? outer : inner, yR = side < 0 ? inner : outer;
      float xa = x0 - 3, xb = x0 + w + 3, yt = y0 - 3;
      float qx[4] = {xa, xb, xb, xa}, qy[4] = {yt, yt, yR, yL};
      aaPolygon(qx, qy, 4, BG, eyeBox);
    }
    if (p.lidBottom > 0) {
      float ry = h * 0.7f, rx = w * 0.78f;
      aaEllipse(ex, y0 + h - p.lidBottom * h + ry, rx, ry, BG, eyeBox);
    }
  }

  // Mouth
  float mx = cx + sx * 0.5f, my = cy + p.eyeH * 0.5f + 26 + p.mouthY + sy * 0.4f, mw = p.mouthW;
  if (p.mouth == MOUTH_CURVE) {
    float px = mx - mw / 2, py = my;
    for (int i = 1; i <= 16; i++) {
      float u = i / 16.0f;
      float nx = (1 - u) * (1 - u) * (mx - mw / 2) + 2 * (1 - u) * u * mx + u * u * (mx + mw / 2);
      float ny = (1 - u) * (1 - u) * my + 2 * (1 - u) * u * (my + p.mouthCurve * 22) + u * u * my;
      spr.drawWideLine(px, py, nx, ny, 2.5f, ec);
      px = nx;
      py = ny;
    }
  } else if (p.mouth == MOUTH_O) {
    float open = (p.fx & FX_MUNCH) ? (0.15f + 0.85f * fabsf(sinf(t / 160.0f))) * p.mouthOpen : p.mouthOpen;
    float rx = 5 + open * 9, ry = 3 + open * 13;
    aaEllipse(mx, my + 4, rx + 2, ry + 2, ec);
    aaEllipse(mx, my + 4, fmaxf(1, rx - 2), fmaxf(1, ry - 2), BG);
  } else if (p.mouth == MOUTH_WAVE) {
    float px = mx - mw / 2, py = my + sinf(t / 140.0f) * 4;
    for (int i = 1; i <= 20; i++) {
      float nx = mx - mw / 2 + mw * i / 20.0f;
      float ny = my + sinf(i / 20.0f * PI * 3 + t / 140.0f) * 4;
      spr.drawWideLine(px, py, nx, ny, 2.0f, ec);
      px = nx;
      py = ny;
    }
  }

  // Extras
  if ((p.fx & FX_BLUSH) && p.blushAlpha > 0) {
    uint16_t bc = scale565(BLUSH_COLOR, p.blushAlpha);
    for (auto& e : eyes)
      aaEllipse(e.ex + e.side * p.blushX, cy + p.eyeH * 0.5f + p.blushY,
                fmaxf(1, p.blushW), fmaxf(1, p.blushH), bc);
  }
  if (p.fx & FX_SWEAT) {
    const Eye& e = eyes[1];
    drop(e.ex + e.w / 2 + 12, cy - p.eyeH * 0.5f + 6 + fmodf(t / 90.0f, 12), 7, DROP);
  }
  if (p.fx & FX_TEARS) {
    for (int i = 0; i < 2; i++) {
      float ph = fmodf(t / 1100.0f + i * 0.5f, 1);
      drop(eyes[i].ex + eyes[i].side * eyes[i].w * 0.25f, cy + p.eyeH * 0.35f + ph * 46, 5,
           scale565(DROP, 1 - ph));
    }
  }
  if (p.fx & FX_ZZZ) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(t / 1700.0f + i / 3.0f, 1);
      letterZ(W - 82 + i * 16 + ph * 8, 74 - ph * 34 - i * 8, 7.0f * (1 + i), 0.45f + 0.4f * i,
              scale565(ec, sinf(ph * PI)));
    }
  }
}

}  // namespace

bool begin(LGFX& display) {
  lcd = &display;
  spr.setPsram(true);
  spr.setColorDepth(16);
  if (!spr.createSprite(W, H)) return false;
  fromFace = FACES[0];
  fromColor = toColor = faceColor(FACES[0]);
  randomSeed(esp_random());
  return true;
}

void show(int index) {
  if (index < 0 || index >= FACE_COUNT) return;
  uint32_t now = millis();
  float k = easeInOut(clampf((now - tweenStart) / (float)TWEEN_MS, 0, 1));
  fromFace = blend(fromFace, FACES[curIdx], k);  // start from whatever is on screen now
  fromColor = mix565(fromColor, toColor, k);
  curIdx = index;
  toColor = faceColor(FACES[index]);
  tweenStart = now;
}

int find(const char* name) {
  for (int i = 0; i < FACE_COUNT; i++)
    if (strcasecmp(name, FACES[i].name) == 0) return i;
  return -1;
}

int current() { return curIdx; }

void update() {
  if (!lcd) return;
  uint32_t now = millis();

  float e = easeInOut(clampf((now - tweenStart) / (float)TWEEN_MS, 0, 1));
  Face f = blend(fromFace, FACES[curIdx], e);
  uint16_t ec = mix565(fromColor, toColor, e);

  // Blink, sometimes twice in a row
  bool canBlink = f.eyeH > 14 && !(f.fx & (FX_HEARTS | FX_SPIRAL));
  if (now >= nextBlink) {
    blinkStart = now;
    nextBlink = now + 2200 + random(4200);
    secondBlink = random(100) < 18 ? now + 260 : 0;
  }
  if (secondBlink && now >= secondBlink) {
    blinkStart = now;
    secondBlink = 0;
  }
  float blink = 0;
  if (canBlink && now - blinkStart < 150) blink = sinf((now - blinkStart) / 150.0f * PI);

  // Idle glances
  if (now >= nextGlance) {
    if (random(100) < 35) {
      gtx = 0;
      gty = 0;
    } else {
      gtx = (random(2001) / 1000.0f - 1) * 0.55f;
      gty = (random(2001) / 1000.0f - 1) * 0.35f;
    }
    nextGlance = now + 1200 + random(2800);
  }
  gx += (gtx - gx) * 0.18f;
  gy += (gty - gy) * 0.18f;

  float bob = sinf(now / 900.0f) * 1.6f;  // breathing

  drawFace(f, ec, now, blink, bob);
  spr.pushSprite(lcd, 0, 0);
}

}  // namespace face
