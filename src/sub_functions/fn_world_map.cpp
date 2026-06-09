/**
 * fn_world_map.cpp — 矢量世界地图（从 SD 卡读取矢量数据）
 * 数据由 convert_geodata.py 生成，存储在 SD 卡 /gpsmap/vector/*.bin
 */
#include "fn_world_map.h"
#include "../display_manager.h"
#include "../gps_manager.h"

static const float COORD_INV = 1.0f / COORD_SCALE;

static void renderPolyArray(VectorReader& reader, uint16_t color,
    float latMin, float latMax, float lonMin, float lonMax,
    float latRange, float lonRange) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int prevSX = -9999, prevSY = -9999;
  bool segActive = false;

  reader.rewind();

  while (true) {
    int16_t v = reader.readNext();

    if (v == 0x7FFF) {
      int16_t next = reader.peekNext();
      if (next == 0x7FFF) { reader.readNext(); break; }  // double sentinel = end of array
      segActive = false;
      continue;
    }

    if (v == 0x7FFE) {
      int16_t bbox[4];
      for (int i = 0; i < 4; i++) bbox[i] = reader.readNext();

      float bbMinLat = bbox[0] * COORD_INV;
      float bbMaxLat = bbox[1] * COORD_INV;
      float bbMinLon = bbox[2] * COORD_INV;
      float bbMaxLon = bbox[3] * COORD_INV;

      if (bbMaxLat < latMin || bbMinLat > latMax ||
          bbMaxLon < lonMin || bbMinLon > lonMax) {
        // bbox doesn't intersect viewport: skip this segment
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
    float lat = latS * COORD_INV;
    float lon = lonS * COORD_INV;

    int sx = (int)((lon - lonMin) / lonRange * SCREEN_W);
    int sy = (int)((latMax - lat) / latRange * SCREEN_H);

    if (segActive) {
      if ((sx > -50 && sx < SCREEN_W + 50 && sy > -50 && sy < SCREEN_H + 50) ||
          (prevSX > -50 && prevSX < SCREEN_W + 50 && prevSY > -50 && prevSY < SCREEN_H + 50)) {
        cv.drawLine(prevSX, prevSY, sx, sy, color);
      }
    }
    prevSX = sx; prevSY = sy;
    segActive = true;
  }
}

void FnWorldMap::onEnter() {
  _zoom = 0;

  if (!SDManager::instance().begin()) {
    _sdReady = false;
    return;
  }

  _sdReady =
    _coastReader.open(PATH_COAST_BIN)   &&
    _borderReader.open(PATH_BORDER_BIN) &&
    _stateReader.open(PATH_STATE_BIN)   &&
    _riverReader.open(PATH_RIVER_BIN)   &&
    _lakeReader.open(PATH_LAKE_BIN);
}

void FnWorldMap::onExit() {
  _coastReader.close();
  _borderReader.close();
  _stateReader.close();
  _riverReader.close();
  _lakeReader.close();
  _sdReady = false;
}

void FnWorldMap::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  if (!_sdReady) {
    cv.fillScreen(TFT_BLACK);
    cv.setTextSize(1);
    cv.setTextColor(TFT_RED);
    cv.setCursor(25, SCREEN_H / 2 - 12);
    cv.print("SD Card not available");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(10, SCREEN_H / 2 + 6);
    cv.print("Insert SD with /gpsmap/vector data");
    return;
  }

  float cLat = gps.hasFix() ? gps.latitude() : 0;
  float cLon = gps.hasFix() ? gps.longitude() : 0;
  bool hasFix = gps.hasFix();

  static const float zoomLon[] = {360, 180, 90, 45, 22.5, 11.25};
  static const float zoomLat[] = {180, 90, 45, 22.5, 11.25, 5.625};
  int z = constrain(_zoom, 0, 5);
  float lonRange = zoomLon[z], latRange = zoomLat[z];
  float latMin, latMax, lonMin, lonMax;
  if (z == 0) { latMin = -90; latMax = 90; lonMin = -180; lonMax = 180; }
  else {
    latMin = cLat - latRange / 2; latMax = cLat + latRange / 2;
    lonMin = cLon - lonRange / 2; lonMax = cLon + lonRange / 2;
  }

  cv.fillScreen(TFT_BLACK);

  // Grid lines
  float gridStep;
  if (lonRange > 180) gridStep = 30;
  else if (lonRange > 90) gridStep = 15;
  else if (lonRange > 45) gridStep = 10;
  else gridStep = 5;
  for (float lat = floor(latMin / gridStep) * gridStep; lat <= latMax; lat += gridStep) {
    int sy = (int)((latMax - lat) / latRange * SCREEN_H);
    if (sy >= 0 && sy < SCREEN_H)
      cv.drawLine(0, sy, SCREEN_W - 1, sy, 0x2104);
  }
  for (float lon = floor(lonMin / gridStep) * gridStep; lon <= lonMax; lon += gridStep) {
    int sx = (int)((lon - lonMin) / lonRange * SCREEN_W);
    if (sx >= 0 && sx < SCREEN_W)
      cv.drawLine(sx, 0, sx, SCREEN_H - 1, 0x2104);
  }
  int eqY = (int)((latMax - 0) / latRange * SCREEN_H);
  if (eqY >= 0 && eqY < SCREEN_H) cv.drawLine(0, eqY, SCREEN_W - 1, eqY, TFT_DARKGREY);
  int pmX = (int)((0 - lonMin) / lonRange * SCREEN_W);
  if (pmX >= 0 && pmX < SCREEN_W) cv.drawLine(pmX, 0, pmX, SCREEN_H - 1, TFT_DARKGREY);

  // Render layers (back to front)
  renderPolyArray(_lakeReader,   0x0B5E, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  renderPolyArray(_riverReader,  0x2B5F, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  renderPolyArray(_stateReader,  0x3BCD, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  renderPolyArray(_borderReader, 0x4A49, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  renderPolyArray(_coastReader,  TFT_GREEN, latMin, latMax, lonMin, lonMax, latRange, lonRange);

  // GPS marker
  if (hasFix) {
    int gx = (int)((cLon - lonMin) / lonRange * SCREEN_W);
    int gy = (int)((latMax - cLat) / latRange * SCREEN_H);
    cv.drawLine(gx - 6, gy, gx + 6, gy, TFT_RED);
    cv.drawLine(gx, gy - 6, gx, gy + 6, TFT_RED);
    cv.fillCircle(gx, gy, 2, TFT_RED);
  }

  // Zoom indicator
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  char zBuf[20];
  snprintf(zBuf, sizeof(zBuf), "z%d [z+/x-] [`]back", z);
  cv.setCursor(2, SCREEN_H - 10);
  cv.print(zBuf);

  if (hasFix) {
    char posBuf[20];
    snprintf(posBuf, sizeof(posBuf), "%.2f,%.2f", cLat, cLon);
    int pw = strlen(posBuf) * 6;
    cv.setCursor(SCREEN_W - pw - 2, SCREEN_H - 10);
    cv.print(posBuf);
  }
}

bool FnWorldMap::onKeyEvent(const KeyEvent& event) {
  if (event.pressed) {
    if (event.key == 'z' && _zoom < 5) { _zoom++; return true; }
    if (event.key == 'x' && _zoom > 0) { _zoom--; return true; }
  }
  return false;
}

void FnWorldMap::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 2;
  // Globe
  cv.drawCircle(cx, cy, r, color);
  // Continents hint — small filled regions
  cv.fillCircle(cx - r/3, cy - r/3, 3, color);
  cv.fillCircle(cx + r/4, cy + r/5, 2, color);
  cv.fillCircle(cx - r/4, cy + r/4, 2, color);
  // Equator
  cv.drawLine(cx - r + 3, cy, cx + r - 3, cy, color);
  // Prime meridian
  cv.drawLine(cx, cy - r + 3, cx, cy + r - 3, color);
}
