#include "ui_helpers.h"
#include "geo_math.h"
#include <math.h>

namespace {

int unit(int size, int pct) {
  return (size * pct + 50) / 100;
}

int px(int origin, int size, int pct) {
  return origin + unit(size, pct);
}

int minIconStroke(int size) {
  int w = unit(size, 10);
  return w < 2 ? 2 : w;
}

void fillCapsuleH(M5Canvas& cv, int x1, int y, int x2, int r, uint16_t color) {
  if (x2 < x1) {
    int t = x1;
    x1 = x2;
    x2 = t;
  }
  if (r < 1) r = 1;
  cv.fillRect(x1, y - r, x2 - x1 + 1, r * 2 + 1, color);
  cv.fillCircle(x1, y, r, color);
  cv.fillCircle(x2, y, r, color);
}

void fillCapsuleV(M5Canvas& cv, int x, int y1, int y2, int r, uint16_t color) {
  if (y2 < y1) {
    int t = y1;
    y1 = y2;
    y2 = t;
  }
  if (r < 1) r = 1;
  cv.fillRect(x - r, y1, r * 2 + 1, y2 - y1 + 1, color);
  cv.fillCircle(x, y1, r, color);
  cv.fillCircle(x, y2, r, color);
}

void thickLine(M5Canvas& cv, int x1, int y1, int x2, int y2,
               int w, uint16_t color) {
  if (w < 1) w = 1;
  int r = w / 2;
  for (int o = -r; o <= r; o++) {
    cv.drawLine(x1 + o, y1, x2 + o, y2, color);
    cv.drawLine(x1, y1 + o, x2, y2 + o, color);
  }
  cv.fillCircle(x1, y1, r, color);
  cv.fillCircle(x2, y2, r, color);
}

void drawGearIcon(M5Canvas& cv, int x, int y, int size,
                  uint16_t color, uint16_t bgColor) {
  int cx = px(x, size, 50);
  int cy = px(y, size, 50);
  int r = unit(size, 22);
  int tooth = unit(size, 9);
  int w = unit(size, 14);

  fillCapsuleV(cv, cx, cy - r - tooth, cy + r + tooth, w / 2, color);
  fillCapsuleH(cv, cx - r - tooth, cy, cx + r + tooth, w / 2, color);
  thickLine(cv, cx - r - tooth / 2, cy - r - tooth / 2,
            cx + r + tooth / 2, cy + r + tooth / 2, w, color);
  thickLine(cv, cx - r - tooth / 2, cy + r + tooth / 2,
            cx + r + tooth / 2, cy - r - tooth / 2, w, color);
  cv.fillCircle(cx, cy, unit(size, 28), color);
  cv.fillCircle(cx, cy, unit(size, 11), bgColor);
}

}  // namespace

void drawMenuIcon(M5Canvas& cv, int iconType, int x, int y, int size,
                  uint16_t color, uint16_t bgColor) {
  int cx = px(x, size, 50);
  int cy = px(y, size, 50);
  int s = size;
  int stroke = minIconStroke(size);

  switch (iconType) {
    case ICON_DASHBOARD: {
      cv.fillRoundRect(px(x, s, 13), px(y, s, 17),
                       unit(s, 74), unit(s, 65), unit(s, 13), color);
      fillCapsuleH(cv, px(x, s, 27), px(y, s, 34),
                   px(x, s, 41), unit(s, 3), bgColor);
      fillCapsuleH(cv, px(x, s, 59), px(y, s, 34),
                   px(x, s, 73), unit(s, 3), bgColor);
      cv.fillCircle(cx, px(y, s, 57), unit(s, 18), bgColor);
      cv.fillCircle(cx, px(y, s, 57), unit(s, 12), color);
      thickLine(cv, cx, px(y, s, 57), px(x, s, 64), px(y, s, 47),
                stroke, bgColor);
      cv.fillCircle(cx, px(y, s, 57), unit(s, 4), bgColor);
      fillCapsuleH(cv, px(x, s, 31), px(y, s, 86),
                   px(x, s, 69), unit(s, 3), color);
      break;
    }

    case ICON_SOS: {
      cv.fillTriangle(cx, px(y, s, 9), px(x, s, 10), px(y, s, 86),
                      px(x, s, 90), px(y, s, 86), color);
      fillCapsuleV(cv, cx, px(y, s, 34), px(y, s, 61),
                   unit(s, 4), bgColor);
      cv.fillCircle(cx, px(y, s, 73), unit(s, 4), bgColor);
      break;
    }

    case ICON_SIGNAL_BARS: {
      int bw = unit(s, 12);
      int gap = unit(s, 7);
      int base = px(y, s, 84);
      int left = px(x, s, 19);
      for (int i = 0; i < 4; i++) {
        int h = unit(s, 24 + i * 13);
        cv.fillRoundRect(left + i * (bw + gap), base - h,
                         bw, h, bw / 2, color);
      }
      break;
    }

    case ICON_TRIP: {
      int w = unit(s, 10);
      thickLine(cv, px(x, s, 24), px(y, s, 72),
                px(x, s, 44), px(y, s, 54), w, color);
      thickLine(cv, px(x, s, 44), px(y, s, 54),
                px(x, s, 64), px(y, s, 62), w, color);
      thickLine(cv, px(x, s, 64), px(y, s, 62),
                px(x, s, 78), px(y, s, 36), w, color);
      cv.fillCircle(px(x, s, 22), px(y, s, 74), unit(s, 12), color);
      cv.fillCircle(px(x, s, 78), px(y, s, 34), unit(s, 12), color);
      cv.fillCircle(px(x, s, 22), px(y, s, 74), unit(s, 4), bgColor);
      cv.fillCircle(px(x, s, 78), px(y, s, 34), unit(s, 4), bgColor);
      break;
    }

    case ICON_BACKTRACK: {
      int w = unit(s, 10);
      fillCapsuleH(cv, px(x, s, 28), px(y, s, 32),
                   px(x, s, 75), w / 2, color);
      fillCapsuleV(cv, px(x, s, 75), px(y, s, 32),
                   px(y, s, 62), w / 2, color);
      fillCapsuleH(cv, px(x, s, 32), px(y, s, 62),
                   px(x, s, 75), w / 2, color);
      cv.fillTriangle(px(x, s, 31), px(y, s, 62),
                      px(x, s, 48), px(y, s, 46),
                      px(x, s, 48), px(y, s, 78), color);
      cv.fillCircle(px(x, s, 75), px(y, s, 32), w / 2, color);
      break;
    }

    case ICON_CLOCK: {
      cv.fillCircle(cx, cy, unit(s, 37), color);
      cv.fillCircle(cx, cy, unit(s, 27), bgColor);
      cv.fillCircle(cx, cy, unit(s, 22), color);
      thickLine(cv, cx, cy, cx, px(y, s, 33), stroke, bgColor);
      thickLine(cv, cx, cy, px(x, s, 65), cy, stroke, bgColor);
      cv.fillCircle(cx, cy, unit(s, 4), bgColor);
      break;
    }

    case ICON_3D_GLOBE: {
      cv.fillEllipse(cx, cy, unit(s, 43), unit(s, 19), color);
      cv.fillEllipse(cx, cy, unit(s, 31), unit(s, 10), bgColor);
      cv.fillCircle(cx, cy, unit(s, 31), color);
      cv.fillCircle(px(x, s, 40), px(y, s, 37), unit(s, 7), bgColor);
      fillCapsuleV(cv, cx, px(y, s, 24), px(y, s, 76),
                   stroke / 2, bgColor);
      fillCapsuleH(cv, px(x, s, 29), cy, px(x, s, 71),
                   stroke / 2, bgColor);
      cv.fillEllipse(px(x, s, 35), cy, unit(s, 6), unit(s, 24), bgColor);
      cv.fillEllipse(px(x, s, 65), cy, unit(s, 6), unit(s, 24), bgColor);
      cv.fillCircle(px(x, s, 76), px(y, s, 27), unit(s, 5), color);
      break;
    }

    case ICON_WORLD_MAP: {
      cv.fillRoundRect(px(x, s, 11), px(y, s, 22),
                       unit(s, 78), unit(s, 56), unit(s, 8), color);
      cv.fillRoundRect(px(x, s, 17), px(y, s, 28),
                       unit(s, 66), unit(s, 44), unit(s, 5), bgColor);
      cv.fillEllipse(px(x, s, 31), px(y, s, 42), unit(s, 10), unit(s, 13),
                     color);
      cv.fillTriangle(px(x, s, 29), px(y, s, 51), px(x, s, 43), px(y, s, 52),
                      px(x, s, 36), px(y, s, 66), color);
      cv.fillEllipse(px(x, s, 61), px(y, s, 42), unit(s, 18), unit(s, 10),
                     color);
      cv.fillTriangle(px(x, s, 52), px(y, s, 48), px(x, s, 75), px(y, s, 52),
                      px(x, s, 61), px(y, s, 64), color);
      cv.fillCircle(px(x, s, 72), px(y, s, 61), unit(s, 5), color);
      fillCapsuleH(cv, px(x, s, 27), px(y, s, 84),
                   px(x, s, 73), unit(s, 3), color);
      break;
    }

    case ICON_OFFLINE_MAP: {
      int top = px(y, s, 18);
      int h = unit(s, 62);
      int w = unit(s, 22);
      cv.fillRoundRect(px(x, s, 13), top, w, h, unit(s, 5), color);
      cv.fillRoundRect(px(x, s, 39), top + unit(s, 8), w, h,
                       unit(s, 5), color);
      cv.fillRoundRect(px(x, s, 65), top, w, h, unit(s, 5), color);
      thickLine(cv, px(x, s, 26), px(y, s, 68),
                px(x, s, 50), px(y, s, 48), stroke, bgColor);
      thickLine(cv, px(x, s, 50), px(y, s, 48),
                px(x, s, 76), px(y, s, 64), stroke, bgColor);
      break;
    }

    case ICON_PEERS: {
      int nodeR = unit(s, 11);
      int topX = cx;
      int topY = px(y, s, 25);
      int leftX = px(x, s, 26);
      int lowY = px(y, s, 67);
      int rightX = px(x, s, 74);
      thickLine(cv, topX, topY, leftX, lowY, stroke, color);
      thickLine(cv, topX, topY, rightX, lowY, stroke, color);
      thickLine(cv, leftX, lowY, rightX, lowY, stroke, color);
      cv.fillCircle(topX, topY, nodeR, color);
      cv.fillCircle(leftX, lowY, nodeR, color);
      cv.fillCircle(rightX, lowY, nodeR, color);
      cv.fillCircle(topX, topY, unit(s, 4), bgColor);
      cv.fillCircle(leftX, lowY, unit(s, 4), bgColor);
      cv.fillCircle(rightX, lowY, unit(s, 4), bgColor);
      fillCapsuleH(cv, px(x, s, 34), px(y, s, 87),
                   px(x, s, 66), unit(s, 3), color);
      break;
    }

    case ICON_WAYPOINT: {
      cv.fillCircle(cx, px(y, s, 35), unit(s, 24), color);
      cv.fillTriangle(px(x, s, 29), px(y, s, 46), px(x, s, 71), px(y, s, 46),
                      cx, px(y, s, 88), color);
      cv.fillCircle(cx, px(y, s, 35), unit(s, 9), bgColor);
      break;
    }

    case ICON_GOTO_NAV: {
      cv.fillCircle(px(x, s, 72), px(y, s, 28), unit(s, 14), color);
      cv.fillCircle(px(x, s, 72), px(y, s, 28), unit(s, 6), bgColor);
      fillCapsuleH(cv, px(x, s, 54), px(y, s, 28),
                   px(x, s, 90), unit(s, 2), color);
      fillCapsuleV(cv, px(x, s, 72), px(y, s, 10),
                   px(y, s, 46), unit(s, 2), color);
      cv.fillTriangle(px(x, s, 18), px(y, s, 78),
                      px(x, s, 45), px(y, s, 19),
                      px(x, s, 62), px(y, s, 86), color);
      cv.fillTriangle(px(x, s, 41), px(y, s, 54),
                      px(x, s, 45), px(y, s, 19),
                      px(x, s, 62), px(y, s, 86), bgColor);
      break;
    }

    case ICON_NMEA: {
      cv.fillRoundRect(px(x, s, 16), px(y, s, 15),
                       unit(s, 68), unit(s, 70), unit(s, 9), color);
      cv.fillTriangle(px(x, s, 65), px(y, s, 15), px(x, s, 84), px(y, s, 34),
                      px(x, s, 65), px(y, s, 34), bgColor);
      fillCapsuleH(cv, px(x, s, 30), px(y, s, 40),
                   px(x, s, 67), stroke / 2, bgColor);
      fillCapsuleH(cv, px(x, s, 30), px(y, s, 55),
                   px(x, s, 70), stroke / 2, bgColor);
      fillCapsuleH(cv, px(x, s, 30), px(y, s, 70),
                   px(x, s, 58), stroke / 2, bgColor);
      break;
    }

    case ICON_SETTINGS:
      drawGearIcon(cv, x, y, size, color, bgColor);
      break;

    case ICON_ABOUT: {
      cv.fillCircle(cx, cy, unit(s, 36), color);
      cv.fillCircle(cx, px(y, s, 31), unit(s, 5), bgColor);
      fillCapsuleV(cv, cx, px(y, s, 45), px(y, s, 71),
                   unit(s, 4), bgColor);
      break;
    }

    default:
      cv.fillCircle(cx, cy, unit(s, 32), color);
      break;
  }
}

uint16_t systemColor(const String& sys) {
  if (sys == "GPS") return TFT_GREEN;
  if (sys == "GLONASS") return TFT_RED;
  if (sys == "Galileo") return TFT_CYAN;
  if (sys == "BeiDou") return TFT_YELLOW;
  if (sys == "QZSS") return TFT_MAGENTA;
  return TFT_WHITE;
}

const char* cardinalFromHeading(float heading) {
  const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)((heading+22.5f)/45.0f) % 8;
  return dirs[idx];
}

float bearingTo(float lat1,float lon1,float lat2,float lon2) {
  return geoBearingDeg(lat1, lon1, lat2, lon2);
}

float haversineKm(float lat1,float lon1,float lat2,float lon2) {
  return geoDistanceKm(lat1, lon1, lat2, lon2);
}

const char* dopQuality(float dop) {
  if(dop<2.0) return "Ideal";
  if(dop<5.0) return "Good";
  if(dop<10.0) return "Fair";
  return "Poor";
}
