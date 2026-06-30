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
      // 速度计表盘：半圆弧表盘 + 刻度 + 指针
      int ro = unit(s, 38), ri = unit(s, 24);
      cv.fillCircle(cx, cy, ro, color);
      cv.fillCircle(cx, cy, ri, bgColor);
      // 切除底部，形成开口付表盘
      cv.fillRect(cx - ro, cy + unit(s, 26), ro * 2 + 2, ro + 2, bgColor);
      // 5 刻度线（210°, 285°, 360°, 75°, 150°）
      for (int i = 0; i < 5; i++) {
        float a = (210.0f + i * 75.0f) * (float)M_PI / 180.0f;
        cv.drawLine(cx + (int)(sinf(a) * ri),       cy - (int)(cosf(a) * ri),
                    cx + (int)(sinf(a) * (ro - 1)), cy - (int)(cosf(a) * (ro - 1)),
                    bgColor);
      }
      // 指针指向 330°（偏低速位置）
      float na = 330.0f * (float)M_PI / 180.0f;
      thickLine(cv, cx, cy,
                cx + (int)(sinf(na) * unit(s, 22)),
                cy - (int)(cosf(na) * unit(s, 22)), stroke, bgColor);
      cv.fillCircle(cx, cy, unit(s, 5), bgColor);
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
      // 地球仪：填充圆 + 纬线 + 经线
      cv.fillCircle(cx, cy, unit(s, 36), color);
      // 三条纬线（赤道 + 南北回归线）
      fillCapsuleH(cv, cx - unit(s, 35), cy,
                   cx + unit(s, 35), unit(s, 2), bgColor);
      fillCapsuleH(cv, cx - unit(s, 27), cy - unit(s, 17),
                   cx + unit(s, 27), unit(s, 2), bgColor);
      fillCapsuleH(cv, cx - unit(s, 27), cy + unit(s, 17),
                   cx + unit(s, 27), unit(s, 2), bgColor);
      // 本初子午线（垂直）
      fillCapsuleV(cv, cx, cy - unit(s, 36), cy + unit(s, 36), unit(s, 2), bgColor);
      // 偏移经线（椭圆环）
      cv.fillEllipse(cx, cy, unit(s, 14), unit(s, 36), bgColor);
      cv.fillEllipse(cx, cy, unit(s, 10), unit(s, 32), color);
      break;
    }

    case ICON_WORLD_MAP: {
      // 矩形地图框 + 简化大陆
      cv.fillRoundRect(px(x, s, 9),  px(y, s, 17),
                       unit(s, 82),  unit(s, 66), unit(s, 4), color);
      cv.fillRect(px(x, s, 13), px(y, s, 21),
                  unit(s, 74), unit(s, 58), bgColor);
      cv.fillEllipse(px(x, s, 25), px(y, s, 37), unit(s, 8),  unit(s, 11), color); // 北美
      cv.fillEllipse(px(x, s, 23), px(y, s, 58), unit(s, 5),  unit(s, 11), color); // 南美
      cv.fillEllipse(px(x, s, 47), px(y, s, 35), unit(s, 6),  unit(s,  8), color); // 欧洲
      cv.fillEllipse(px(x, s, 48), px(y, s, 54), unit(s, 6),  unit(s, 13), color); // 非洲
      cv.fillEllipse(px(x, s, 67), px(y, s, 33), unit(s, 14), unit(s, 11), color); // 亚洲
      cv.fillCircle( px(x, s, 73), px(y, s, 57), unit(s, 5),              color);  // 澳洲
      break;
    }

    case ICON_OFFLINE_MAP: {
      // 带折角的地图 + 下载箭头
      int foldSz = unit(s, 18);
      int mTopX  = px(x, s, 13), mTopY = px(y, s, 17);
      int mW     = unit(s, 74),  mH    = unit(s, 66);
      cv.fillRect(mTopX,               mTopY,          mW - foldSz, mH,          color);
      cv.fillRect(mTopX + mW - foldSz, mTopY + foldSz, foldSz,      mH - foldSz, color);
      cv.fillTriangle(mTopX + mW - foldSz, mTopY,
                      mTopX + mW,          mTopY + foldSz,
                      mTopX + mW - foldSz, mTopY + foldSz, bgColor);
      // 下载箭头（bgColor）
      int arCX  = cx;
      int arTop = mTopY + unit(s, 8);
      int arBot = mTopY + unit(s, 38);
      fillCapsuleV(cv, arCX, arTop, arBot, unit(s, 4), bgColor);
      cv.fillTriangle(arCX - unit(s, 10), arBot,
                      arCX + unit(s, 10), arBot,
                      arCX,               arBot + unit(s, 12), bgColor);
      fillCapsuleH(cv, arCX - unit(s, 12), arBot + unit(s, 14),
                   arCX + unit(s, 12),     unit(s, 3), bgColor);
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
      // 导航游标（GPS 定位光标形状）
      cv.fillTriangle(px(x, s, 52), px(y, s, 12),
                      px(x, s, 13), px(y, s, 86),
                      px(x, s, 66), px(y, s, 72), color);
      cv.fillTriangle(px(x, s, 13), px(y, s, 86),
                      px(x, s, 66), px(y, s, 72),
                      px(x, s, 42), px(y, s, 55), bgColor);
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

    case ICON_COMPASS: {
      // 指南针：表圈 + 表面 + 刻度 + 南北针
      cv.fillCircle(cx, cy, unit(s, 36), color);   // 表圈
      cv.fillCircle(cx, cy, unit(s, 26), bgColor); // 表面背景
      // 四个基数刻度（color 短线）
      int rTi = unit(s, 19), rTo = unit(s, 25);
      for (int i = 0; i < 4; i++) {
        float a  = i * 90.0f * (float)M_PI / 180.0f;
        int lx1 = cx + (int)(sinf(a) * rTi), ly1 = cy - (int)(cosf(a) * rTi);
        int lx2 = cx + (int)(sinf(a) * rTo), ly2 = cy - (int)(cosf(a) * rTo);
        cv.drawLine(lx1,     ly1, lx2,     ly2, color);
        cv.drawLine(lx1 + 1, ly1, lx2 + 1, ly2, color);
      }
      // 北针（朝上，较长）
      int nTip = unit(s, 22), nW = unit(s, 9);
      cv.fillTriangle(cx, cy - nTip, cx - nW, cy, cx + nW, cy, color);
      // 南针（朝下，略短）
      int sTail = unit(s, 16), sW = unit(s, 7);
      cv.fillTriangle(cx, cy + sTail, cx - sW, cy, cx + sW, cy, color);
      cv.fillCircle(cx, cy, unit(s, 5), bgColor);
      break;
    }

    case ICON_TWILIGHT: {
      // 日出图标：地平线 + 上升半圆 + 三条光芒
      int hY = cy + unit(s, 12);   // 地平线位置（中心偏下）
      int r  = unit(s, 22);        // 太阳半径

      // 地平线（两段，中间留出太阳）
      cv.fillRect(px(x, s, 8),  hY - 1, cx - r - 2 - px(x, s, 8),        3, color);
      cv.fillRect(cx + r + 2,   hY - 1, px(x, s, 92) - (cx + r + 2),     3, color);

      // 上半圆太阳（地平线以上）
      cv.fillCircle(cx, hY, r, color);
      cv.fillRect(px(x, s, 8), hY + 1, unit(s, 84), r + 4, bgColor);  // 遮掉下半

      // 内圆镂空
      cv.fillCircle(cx, hY, unit(s, 12), bgColor);

      // 光芒（3 条）
      int rayInner = r + unit(s, 5);
      int rayOuter = r + unit(s, 14);
      // 正上方
      cv.drawLine(cx, hY - rayInner, cx, hY - rayOuter, color);
      cv.drawLine(cx + 1, hY - rayInner, cx + 1, hY - rayOuter, color);
      // 左上 45°
      float a45 = 0.7854f;
      int lx1 = cx - (int)(sinf(a45) * rayInner), ly1 = hY - (int)(cosf(a45) * rayInner);
      int lx2 = cx - (int)(sinf(a45) * rayOuter), ly2 = hY - (int)(cosf(a45) * rayOuter);
      cv.drawLine(lx1, ly1, lx2, ly2, color);
      // 右上 45°
      int rx1 = cx + (int)(sinf(a45) * rayInner);
      int rx2 = cx + (int)(sinf(a45) * rayOuter);
      cv.drawLine(rx1, ly1, rx2, ly2, color);
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
