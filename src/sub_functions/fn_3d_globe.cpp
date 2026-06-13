/**
 * fn_3d_globe.cpp — 3D旋转地球 + 卫星轨道可视化（从 SD 卡读取低分辨率矢量数据）
 * 数据由 convert_geodata.py 生成，存储在 SD 卡 /gpstoolkit/vector/coast_low.bin
 */
#include "fn_3d_globe.h"
#include "../display_manager.h"
#include "../gps_manager.h"

static float _gSin[91];
static bool  _gSinReady = false;

static float qsin(int cd) {
  cd = ((cd % 36000) + 36000) % 36000;
  bool neg = false;
  if (cd >= 18000) { cd -= 18000; neg = true; }
  if (cd > 9000)   cd = 18000 - cd;
  int d = cd / 100, f = cd % 100;
  float s = _gSin[d];
  if (f && d < 90) s += (_gSin[d + 1] - s) * f * 0.01f;
  return neg ? -s : s;
}
static float qcos(int cd) { return qsin(cd + 9000); }

#define GLOBE_PROJ(latCD, lonCD, r, sx_, sy_, z_) do { \
  float sl_ = qsin(latCD), cl_ = qcos(latCD);          \
  float sn_ = qsin(lonCD), cn_ = qcos(lonCD);          \
  float x_ = (r)*cl_*sn_, y_ = (r)*sl_, zz_ = (r)*cl_*cn_; \
  float x2_ = x_*cc + zz_*sc;                          \
  float z2_ = -x_*sc + zz_*cc;                         \
  float y2_ = y_*ct - z2_*st;                           \
  (z_) = y_*st + z2_*ct;                                \
  (sx_) = gcx + (int)(x2_ * ER);                       \
  (sy_) = gcy - (int)(y2_ * ER);                       \
} while(0)

static void globeRenderLayer(VectorReader& reader,
    float sc, float cc, float st, float ct,
    int gcx, int gcy, float ER, int camLon100,
    uint16_t colBright, uint16_t colMid, uint16_t colDim) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int psx = 0, psy = 0; float pz = -1;
  bool segActive = false;

  reader.rewind();

  while (true) {
    int16_t v = reader.readNext();

    if (v == 0x7FFF) {
      int16_t next = reader.peekNext();
      if (next == 0x7FFF) { reader.readNext(); break; }
      segActive = false;
      continue;
    }

    if (v == 0x7FFE) {
      int16_t bLonMin = reader.readNext();   // bbox[0] = minLat (skip)
      int16_t bLonMax = reader.readNext();   // bbox[1] = maxLat (skip)
      bLonMin = reader.readNext();            // bbox[2] = minLon
      bLonMax = reader.readNext();            // bbox[3] = maxLon

      int mid = (bLonMin + bLonMax) / 2;
      int span = (bLonMax - bLonMin) / 2;
      int rel = mid + camLon100;
      if (rel >  18000) rel -= 36000;
      if (rel < -18000) rel += 36000;
      if (rel < 0) rel = -rel;

      if (rel - span > 9500) {
        // Back-facing segment: skip
        while (true) {
          int16_t sv = reader.readNext();
          if (sv == 0x7FFF) break;
        }
        segActive = false;
        continue;
      }
      segActive = false;
      continue;
    }

    // Regular coordinate pair
    int16_t latS = v;
    int16_t lonS = reader.readNext();

    int sx, sy; float z;
    GLOBE_PROJ(latS, lonS, 1.0f, sx, sy, z);

    if (segActive && z > 0 && pz > 0) {
      if ((unsigned)sx < (unsigned)SCREEN_W && (unsigned)sy < (unsigned)SCREEN_H &&
          (unsigned)psx < (unsigned)SCREEN_W && (unsigned)psy < (unsigned)SCREEN_H) {
        float zAvg = (z + pz) * 0.5f;
        uint16_t c = (zAvg > 0.50f) ? colBright : (zAvg > 0.18f) ? colMid : colDim;
        cv.drawLine(psx, psy, sx, sy, c);
      }
    }
    psx = sx; psy = sy; pz = z;
    segActive = true;
  }
}

void Fn3DGlobe::onEnter() {
  if (!_gSinReady) {
    for (int i = 0; i <= 90; i++) _gSin[i] = sinf(i * DEG_TO_RAD);
    _gSinReady = true;
  }
  _camAngle = 0;
  _lastMs = 0;

  if (!SDManager::instance().begin()) {
    _sdReady = false;
    return;
  }
  _sdReady = _coastLowReader.open(PATH_COAST_LOW_BIN);
  _borderLowReader.open(PATH_BORDER_LOW_BIN);
}

void Fn3DGlobe::onExit() {
  _coastLowReader.close();
  _borderLowReader.close();
  _sdReady = false;
}

void Fn3DGlobe::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  if (!_sdReady) {
    cv.fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, TFT_BLACK);
    cv.setTextSize(1);
    cv.setTextColor(TFT_RED);
    cv.setCursor(30, CONTENT_TOP + CONTENT_H / 2 - 10);
    cv.print("SD Card not available");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(8, CONTENT_TOP + CONTENT_H / 2 + 6);
    cv.print("Insert SD with /gpstoolkit/vector data");
    return;
  }

  unsigned long now = millis();
  float dt = (_lastMs == 0) ? 0.05f : (now - _lastMs) / 1000.0f;
  if (dt > 0.5f) dt = 0.05f;
  _lastMs = now;
  _camAngle += 12.0f * dt;
  if (_camAngle >= 360.0f) _camAngle -= 360.0f;

  bool hasLoc = gps.hasReliableFix();
  float centerLon = hasLoc ? gps.longitude() : 0;
  float centerLat = hasLoc ? gps.latitude() : 20.0f;
  float camLonDeg = centerLon + _camAngle;
  float camTiltDeg = 22.0f + centerLat * 0.35f;
  float camLonRad = camLonDeg * DEG_TO_RAD, camTiltRad = camTiltDeg * DEG_TO_RAD;
  float sc = sinf(camLonRad), cc = cosf(camLonRad);
  float st = sinf(camTiltRad), ct = cosf(camTiltRad);

  const int gcx = SCREEN_W / 2, gcy = CONTENT_TOP + CONTENT_H / 2;
  const float ER = 28.0f;

  int camLon100 = (int)(camLonDeg * 100) % 36000;
  if (camLon100 < -18000) camLon100 += 36000;
  if (camLon100 >  18000) camLon100 -= 36000;

  cv.fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, TFT_BLACK);

  // Ocean + atmosphere
  cv.fillCircle(gcx, gcy, (int)ER, 0x0008);
  cv.drawCircle(gcx, gcy, (int)ER + 2, 0x0011);
  cv.drawCircle(gcx, gcy, (int)ER + 1, 0x001A);

  // Lat/lon grid
  const uint16_t gridCol = 0x0926;
  for (int latD = -60; latD <= 60; latD += 30) {
    int ppx = -1, ppy = -1; float ppz = -1; bool first = true;
    for (int lonD = -180; lonD <= 180; lonD += 12) {
      int sx, sy; float z;
      GLOBE_PROJ(latD * 100, lonD * 100, 1.0f, sx, sy, z);
      if (!first && z > 0 && ppz > 0) cv.drawLine(ppx, ppy, sx, sy, gridCol);
      ppx = sx; ppy = sy; ppz = z; first = false;
    }
  }
  for (int lonD = 0; lonD < 360; lonD += 30) {
    int lonCD = (lonD <= 180 ? lonD : lonD - 360) * 100;
    int ppx = -1, ppy = -1; float ppz = -1; bool first = true;
    for (int latD = -90; latD <= 90; latD += 12) {
      int sx, sy; float z;
      GLOBE_PROJ(latD * 100, lonCD, 1.0f, sx, sy, z);
      if (!first && z > 0 && ppz > 0) cv.drawLine(ppx, ppy, sx, sy, gridCol);
      ppx = sx; ppy = sy; ppz = z; first = false;
    }
  }

  // Coastlines (simplified data)
  globeRenderLayer(_coastLowReader,
      sc, cc, st, ct, gcx, gcy, ER, camLon100,
      0x07C0, 0x0380, 0x01C0);

  if (_borderLowReader.isOpen()) {
    globeRenderLayer(_borderLowReader,
        sc, cc, st, ct, gcx, gcy, ER, camLon100,
        0x4A49, 0x2924, 0x1482);
  }

  if (hasLoc) {
    int ux, uy; float uz;
    int uLatCD = (int)(gps.latitude() * 100);
    int uLonCD = (int)(gps.longitude() * 100);
    GLOBE_PROJ(uLatCD, uLonCD, 1.0f, ux, uy, uz);
    if (uz > 0) {
      cv.drawLine(ux - 6, uy, ux + 6, uy, TFT_RED);
      cv.drawLine(ux, uy - 6, ux, uy + 6, TFT_RED);
      cv.fillCircle(ux, uy, 2, TFT_RED);
    }
  }

  // --- Draw satellites at correct orbital positions ---
  // Count all visible satellites per system to match Signal/Constellation.
  int gpsCnt = 0, glnCnt = 0, galCnt = 0, bdsCnt = 0, qzssCnt = 0;
  {
    const auto& csats = gps.satellites();
    for (size_t i = 0; i < csats.size(); i++) {
      if (!csats[i].visible) continue;
      if      (csats[i].system == "GPS")     gpsCnt++;
      else if (csats[i].system == "GLONASS") glnCnt++;
      else if (csats[i].system == "Galileo") galCnt++;
      else if (csats[i].system == "BeiDou")  bdsCnt++;
      else if (csats[i].system == "QZSS")    qzssCnt++;
    }
  }

  if (hasLoc) {
    float uLatR = gps.latitude() * DEG_TO_RAD;
    float uLonR = gps.longitude() * DEG_TO_RAD;
    float sinUlat = sinf(uLatR), cosUlat = cosf(uLatR);
    const auto& sats = gps.satellites();

    for (size_t i = 0; i < sats.size(); i++) {
      const auto& sat = sats[i];
      if (!sat.visible || sat.elevation < 1) continue;

      float orbitR; uint16_t col;
      if      (sat.system == "GPS")     { orbitR = 4.17f; col = TFT_YELLOW;  }
      else if (sat.system == "GLONASS") { orbitR = 4.00f; col = TFT_CYAN;    }
      else if (sat.system == "Galileo") { orbitR = 4.65f; col = 0x54BF;      }
      else if (sat.system == "BeiDou")  { orbitR = 4.38f; col = TFT_ORANGE;  }
      else if (sat.system == "QZSS")    { orbitR = 4.17f; col = TFT_MAGENTA; }
      else                                { orbitR = 4.17f; col = TFT_WHITE;   }

      float visR = 1.0f + (orbitR - 1.0f) * 0.22f;
      float elR = sat.elevation * DEG_TO_RAD;
      float azR = sat.azimuth   * DEG_TO_RAD;
      float nadirAng = asinf(cosf(elR) / orbitR);
      float geocAng  = (float)M_PI * 0.5f - elR - nadirAng;
      float sinG = sinf(geocAng), cosG = cosf(geocAng);
      float sinAz = sinf(azR), cosAz = cosf(azR);
      float satLat = asinf(sinUlat * cosG + cosUlat * sinG * cosAz);
      float satLon = uLonR + atan2f(sinAz * sinG * cosUlat,
                                     cosG - sinUlat * sinf(satLat));

      float cla = cosf(satLat), sla = sinf(satLat);
      float slo = sinf(satLon), clo = cosf(satLon);

      int spx, spy, epx, epy; float spz, epz;
      { // proj3(visR, spx, spy, spz)
        float x = visR * cla * slo, y = visR * sla, zz = visR * cla * clo;
        float x2 = x * cc + zz * sc, z2 = -x * sc + zz * cc;
        float y2 = y * ct - z2 * st;
        spz = y * st + z2 * ct;
        spx = gcx + (int)(x2 * ER);
        spy = gcy - (int)(y2 * ER);
      }
      { // proj3(1.0f, epx, epy, epz)
        float x = 1.0f * cla * slo, y = 1.0f * sla, zz = 1.0f * cla * clo;
        float x2 = x * cc + zz * sc, z2 = -x * sc + zz * cc;
        float y2 = y * ct - z2 * st;
        epz = y * st + z2 * ct;
        epx = gcx + (int)(x2 * ER);
        epy = gcy - (int)(y2 * ER);
      }

      // Tether line
      if (spz > 0 && epz > 0)
        cv.drawLine(epx, epy, spx, spy, 0x2104);

      // Satellite dot
      if (spz > 0 && (unsigned)spx < (unsigned)SCREEN_W && (unsigned)spy < (unsigned)SCREEN_H) {
        int r = (sat.snr > 30) ? 3 : (sat.snr > 15) ? 2 : 1;
        cv.fillCircle(spx, spy, r, col);
      }
    }
  }

  // Legend — left column, with counts
  int ly = SCREEN_H - 40;
  char lbuf[16];
  cv.setTextSize(1);
  cv.setTextColor(TFT_YELLOW); snprintf(lbuf,sizeof(lbuf),"GPS %d",gpsCnt); cv.setCursor(2,ly); cv.print(lbuf);
  cv.setTextColor(TFT_CYAN);   snprintf(lbuf,sizeof(lbuf),"GLN %d",glnCnt); cv.setCursor(2,ly+8); cv.print(lbuf);
  cv.setTextColor(0x54BF);      snprintf(lbuf,sizeof(lbuf),"GAL %d",galCnt); cv.setCursor(2,ly+16); cv.print(lbuf);
  cv.setTextColor(TFT_ORANGE); snprintf(lbuf,sizeof(lbuf),"BDS %d",bdsCnt); cv.setCursor(2,ly+24); cv.print(lbuf);
  cv.setTextColor(TFT_MAGENTA); snprintf(lbuf,sizeof(lbuf),"QZS %d",qzssCnt); cv.setCursor(2,ly+32); cv.print(lbuf);

  // Top line — lat/lon
  if (hasLoc) {
    snprintf(lbuf, sizeof(lbuf), "%.4f %.4f", gps.latitude(), gps.longitude());
    cv.setTextColor(TFT_WHITE);
    cv.setCursor((SCREEN_W - (int)strlen(lbuf)*6)/2, 1);
    cv.print(lbuf);
  }
}

void Fn3DGlobe::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 2;
  // Globe outline
  cv.drawCircle(cx, cy, r, color);
  // Tilted ellipse for 3D effect
  cv.drawEllipse(cx, cy, r * 2 - 4, r / 3, color);
  // Vertical axis
  cv.drawLine(cx, cy - r, cx, cy + r, color);
}
