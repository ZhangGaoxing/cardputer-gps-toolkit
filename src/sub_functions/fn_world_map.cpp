/**
 * fn_world_map.cpp - Vector world map loaded from SD card.
 *
 * Vector data lives under /gpstoolkit/vector on the SD card. The .bin files keep
 * the original Cardputer-Adv-GPS-Info coordinate stream, while .idx files add
 * per-segment bbox/offset records so zoomed views do not need to scan megabytes
 * of off-screen geometry.
 */
#include "fn_world_map.h"
#include "../display_manager.h"
#include "../gps_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float COORD_INV = 1.0f / COORD_SCALE;

struct MapCitySd {
  int16_t lat10;
  int16_t lon10;
  char name[12];
};

static bool openVectorFile(VectorReader& reader, const char* primary, const char* fileName) {
  if (reader.open(primary)) return true;

  char path[48];
  snprintf(path, sizeof(path), PATH_BASE "/%s", fileName);
  if (reader.open(path)) return true;

  snprintf(path, sizeof(path), "/vector_bin/%s", fileName);
  return reader.open(path);
}

static bool openVectorIndex(VectorIndexReader& reader, const char* primary, const char* fileName) {
  if (reader.open(primary)) return true;

  char path[48];
  snprintf(path, sizeof(path), PATH_BASE "/%s", fileName);
  if (reader.open(path)) return true;

  snprintf(path, sizeof(path), "/vector_bin/%s", fileName);
  return reader.open(path);
}

static File openCityFile() {
  File f = SD.open(PATH_CITIES_BIN, FILE_READ);
  if (f) return f;
  f = SD.open(PATH_BASE "/cities.bin", FILE_READ);
  if (f) return f;
  return SD.open("/vector_bin/cities.bin", FILE_READ);
}

static bool vectorFileExists(const char* primary, const char* fileName) {
  VectorReader reader;
  bool ok = openVectorFile(reader, primary, fileName);
  reader.close();
  return ok;
}

static bool vectorIndexExists(const char* primary, const char* fileName) {
  VectorIndexReader reader;
  bool ok = openVectorIndex(reader, primary, fileName);
  reader.close();
  return ok;
}

static bool cityFileExists() {
  File f = openCityFile();
  if (!f) return false;
  f.close();
  return true;
}

static bool bboxIntersects(const VectorSegmentIndexEntry& e,
                           int16_t latMinS, int16_t latMaxS,
                           int16_t lonMinS, int16_t lonMaxS) {
  return !(e.maxLat < latMinS || e.minLat > latMaxS ||
           e.maxLon < lonMinS || e.minLon > lonMaxS);
}

static bool pointNearScreen(int x, int y) {
  return x > -50 && x < SCREEN_W + 50 && y > -50 && y < SCREEN_H + 50;
}

static void drawIndexedSegment(VectorReader& reader, uint16_t color,
                               uint16_t pointCount,
                               float latMaxS, float lonMinS,
                               float sxScale, float syScale) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int prevSX = -9999;
  int prevSY = -9999;
  bool segActive = false;

  for (uint16_t i = 0; i < pointCount; i++) {
    int16_t latS = reader.readNext();
    int16_t lonS = reader.readNext();
    if (latS == 0x7FFF || lonS == 0x7FFF) break;

    int sx = (int)((lonS - lonMinS) * sxScale);
    int sy = (int)((latMaxS - latS) * syScale);

    if (segActive && (sx != prevSX || sy != prevSY)) {
      if (pointNearScreen(sx, sy) || pointNearScreen(prevSX, prevSY)) {
        cv.drawLine(prevSX, prevSY, sx, sy, color);
      }
    }

    prevSX = sx;
    prevSY = sy;
    segActive = true;
  }
}

static bool renderPolyIndexed(VectorReader& reader, VectorIndexReader& index,
                              uint16_t color,
                              float latMin, float latMax,
                              float lonMin, float lonMax,
                              float latRange, float lonRange) {
  if (!reader.isOpen() || !index.isOpen()) return false;

  const int16_t latMinS = (int16_t)floorf(latMin * COORD_SCALE);
  const int16_t latMaxS = (int16_t)ceilf(latMax * COORD_SCALE);
  const int16_t lonMinS = (int16_t)floorf(lonMin * COORD_SCALE);
  const int16_t lonMaxS = (int16_t)ceilf(lonMax * COORD_SCALE);
  const float latMaxScaled = latMax * COORD_SCALE;
  const float lonMinScaled = lonMin * COORD_SCALE;
  const float sxScale = (float)SCREEN_W / (lonRange * COORD_SCALE);
  const float syScale = (float)SCREEN_H / (latRange * COORD_SCALE);

  index.rewind();

  VectorSegmentIndexEntry entry;
  while (index.readNext(entry)) {
    if (!bboxIntersects(entry, latMinS, latMaxS, lonMinS, lonMaxS)) continue;
    if (entry.pointCount < 2) continue;
    if (!reader.seek(entry.dataOffset)) continue;
    drawIndexedSegment(reader, color, entry.pointCount,
                       latMaxScaled, lonMinScaled, sxScale, syScale);
  }

  return true;
}

static void renderPolyArray(VectorReader& reader, uint16_t color,
                            float latMin, float latMax,
                            float lonMin, float lonMax,
                            float latRange, float lonRange) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int prevSX = -9999;
  int prevSY = -9999;
  bool segActive = false;
  const float latMaxScaled = latMax * COORD_SCALE;
  const float lonMinScaled = lonMin * COORD_SCALE;
  const float sxScale = (float)SCREEN_W / (lonRange * COORD_SCALE);
  const float syScale = (float)SCREEN_H / (latRange * COORD_SCALE);

  reader.rewind();

  while (true) {
    int16_t v = reader.readNext();

    if (v == 0x7FFF) {
      int16_t next = reader.peekNext();
      if (next == 0x7FFF) {
        reader.readNext();
        break;
      }
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

    int16_t latS = v;
    int16_t lonS = reader.readNext();

    int sx = (int)((lonS - lonMinScaled) * sxScale);
    int sy = (int)((latMaxScaled - latS) * syScale);

    if (segActive && (sx != prevSX || sy != prevSY)) {
      if (pointNearScreen(sx, sy) || pointNearScreen(prevSX, prevSY)) {
        cv.drawLine(prevSX, prevSY, sx, sy, color);
      }
    }

    prevSX = sx;
    prevSY = sy;
    segActive = true;
  }
}

static void renderLayer(VectorReader& reader, VectorIndexReader& index,
                        uint16_t color,
                        float latMin, float latMax,
                        float lonMin, float lonMax,
                        float latRange, float lonRange) {
  if (!renderPolyIndexed(reader, index, color,
                         latMin, latMax, lonMin, lonMax,
                         latRange, lonRange)) {
    renderPolyArray(reader, color, latMin, latMax, lonMin, lonMax,
                    latRange, lonRange);
  }
}

static void renderLayerFromSd(const char* binPath, const char* binName,
                              const char* idxPath, const char* idxName,
                              uint16_t color,
                              float latMin, float latMax,
                              float lonMin, float lonMax,
                              float latRange, float lonRange) {
  VectorReader reader;
  if (!openVectorFile(reader, binPath, binName)) return;

  VectorIndexReader index;
  openVectorIndex(index, idxPath, idxName);

  renderLayer(reader, index, color,
              latMin, latMax, lonMin, lonMax, latRange, lonRange);

  index.close();
  reader.close();
}

static int cityNameLen(const char* name) {
  int len = 0;
  while (len < 12 && name[len] != '\0') len++;
  return len;
}

static void renderCities(File& cityFile, int z,
                         float latMin, float latMax,
                         float lonMin, float lonMax,
                         float latRange, float lonRange) {
  if (!cityFile) return;

  M5Canvas& cv = DisplayManager::instance().canvas();
  const float sxScale = (float)SCREEN_W / (lonRange * COORD_SCALE);
  const float syScale = (float)SCREEN_H / (latRange * COORD_SCALE);
  const float latMaxScaled = latMax * COORD_SCALE;
  const float lonMinScaled = lonMin * COORD_SCALE;

  cityFile.seek(0);

  MapCitySd city;
  while (cityFile.read((uint8_t*)&city, sizeof(city)) == sizeof(city)) {
    float clat = city.lat10 * COORD_INV;
    float clon = city.lon10 * COORD_INV;
    if (clat < latMin || clat > latMax || clon < lonMin || clon > lonMax) {
      continue;
    }

    int cx = (int)((city.lon10 - lonMinScaled) * sxScale);
    int cy = (int)((latMaxScaled - city.lat10) * syScale);
    if (cx < 1 || cx >= SCREEN_W - 1 || cy < 1 || cy >= SCREEN_H - 1) {
      continue;
    }

    cv.fillCircle(cx, cy, 1, TFT_YELLOW);

    if (z >= 4) {
      char name[13];
      memcpy(name, city.name, 12);
      name[12] = '\0';
      int len = cityNameLen(name);
      if (len == 0) continue;

      int tx = cx + 3;
      if (tx + len * 6 > SCREEN_W) tx = cx - len * 6 - 2;
      if (tx < 0) tx = 0;
      int ty = cy - 3;
      if (ty < 0) ty = 0;
      if (ty > SCREEN_H - 8) ty = SCREEN_H - 8;

      cv.setTextColor(TFT_WHITE);
      cv.setCursor(tx, ty);
      cv.print(name);
    }
  }
}

static void renderCitiesFromSd(int z,
                               float latMin, float latMax,
                               float lonMin, float lonMax,
                               float latRange, float lonRange) {
  File cityFile = openCityFile();
  if (!cityFile) return;
  renderCities(cityFile, z, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  cityFile.close();
}

void FnWorldMap::onEnter() {
  _zoom = 0;
  _sdReady = false;
  _indexReady = false;
  _citiesReady = false;
  _coastReady = false;
  _borderReady = false;
  _stateReady = false;
  _riverReady = false;
  _lakeReady = false;
  _errorMsg = nullptr;

  if (!SDManager::instance().begin()) {
    _errorMsg = "SD mount failed";
    drawMap(true);
    DisplayManager::instance().commit();
    return;
  }

  _coastReady = vectorFileExists(PATH_COAST_BIN, "coast.bin");
  _borderReady = vectorFileExists(PATH_BORDER_BIN, "border.bin");
  _stateReady = vectorFileExists(PATH_STATE_BIN, "state.bin");
  _riverReady = vectorFileExists(PATH_RIVER_BIN, "river.bin");
  _lakeReady = vectorFileExists(PATH_LAKE_BIN, "lake.bin");
  bool coreReady = _coastReady || _borderReady;

  _citiesReady = cityFileExists();

  bool coastIdxReady = vectorIndexExists(PATH_COAST_IDX, "coast.idx");
  bool borderIdxReady = vectorIndexExists(PATH_BORDER_IDX, "border.idx");
  bool stateIdxReady = vectorIndexExists(PATH_STATE_IDX, "state.idx");
  bool riverIdxReady = vectorIndexExists(PATH_RIVER_IDX, "river.idx");
  bool lakeIdxReady = vectorIndexExists(PATH_LAKE_IDX, "lake.idx");
  _indexReady = coastIdxReady && borderIdxReady && stateIdxReady &&
                riverIdxReady && lakeIdxReady;

  _sdReady = coreReady;
  if (!_sdReady) {
    _errorMsg = "Need coast/border .bin";
  }

  drawMap(true);
  DisplayManager::instance().commit();
}

void FnWorldMap::onExit() {
  _sdReady = false;
  _indexReady = false;
  _citiesReady = false;
  _coastReady = false;
  _borderReady = false;
  _stateReady = false;
  _riverReady = false;
  _lakeReady = false;
  _errorMsg = nullptr;
}

void FnWorldMap::drawMap(bool force) {
  (void)force;

  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  if (!_sdReady) {
    cv.fillScreen(TFT_BLACK);
    cv.setTextSize(1);
    cv.setTextColor(TFT_RED);
    cv.setCursor(20, SCREEN_H / 2 - 18);
    cv.print(_errorMsg ? _errorMsg : "World map unavailable");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(6, SCREEN_H / 2 - 2);
    cv.print("Copy /gpstoolkit/vector to SD");
    cv.setCursor(6, SCREEN_H / 2 + 10);
    cv.print("Need coast.bin or border.bin");
    return;
  }

  float cLat = gps.hasFix() ? gps.latitude() : 0;
  float cLon = gps.hasFix() ? gps.longitude() : 0;
  bool hasFix = gps.hasFix();

  static const float zoomLon[] = {360, 180, 90, 45, 22.5, 11.25};
  static const float zoomLat[] = {180, 90, 45, 22.5, 11.25, 5.625};
  int z = constrain(_zoom, 0, 5);
  float lonRange = zoomLon[z];
  float latRange = zoomLat[z];
  float latMin, latMax, lonMin, lonMax;
  if (z == 0) {
    latMin = -90;
    latMax = 90;
    lonMin = -180;
    lonMax = 180;
  } else {
    latMin = cLat - latRange / 2;
    latMax = cLat + latRange / 2;
    lonMin = cLon - lonRange / 2;
    lonMax = cLon + lonRange / 2;
  }

  cv.fillScreen(TFT_BLACK);

  float gridStep;
  if (lonRange > 180) gridStep = 30;
  else if (lonRange > 90) gridStep = 15;
  else if (lonRange > 45) gridStep = 10;
  else gridStep = 5;

  for (float lat = floor(latMin / gridStep) * gridStep; lat <= latMax; lat += gridStep) {
    int sy = (int)((latMax - lat) / latRange * SCREEN_H);
    if (sy >= 0 && sy < SCREEN_H) cv.drawLine(0, sy, SCREEN_W - 1, sy, 0x2104);
  }
  for (float lon = floor(lonMin / gridStep) * gridStep; lon <= lonMax; lon += gridStep) {
    int sx = (int)((lon - lonMin) / lonRange * SCREEN_W);
    if (sx >= 0 && sx < SCREEN_W) cv.drawLine(sx, 0, sx, SCREEN_H - 1, 0x2104);
  }

  int eqY = (int)((latMax - 0) / latRange * SCREEN_H);
  if (eqY >= 0 && eqY < SCREEN_H) cv.drawLine(0, eqY, SCREEN_W - 1, eqY, TFT_DARKGREY);
  int pmX = (int)((0 - lonMin) / lonRange * SCREEN_W);
  if (pmX >= 0 && pmX < SCREEN_W) cv.drawLine(pmX, 0, pmX, SCREEN_H - 1, TFT_DARKGREY);

  if (z >= 2 && _lakeReady) {
    renderLayerFromSd(PATH_LAKE_BIN, "lake.bin",
                      PATH_LAKE_IDX, "lake.idx",
                      0x0B5E, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }
  if (z >= 2 && _riverReady) {
    renderLayerFromSd(PATH_RIVER_BIN, "river.bin",
                      PATH_RIVER_IDX, "river.idx",
                      0x2B5F, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }
  if (z >= 3 && _stateReady) {
    renderLayerFromSd(PATH_STATE_BIN, "state.bin",
                      PATH_STATE_IDX, "state.idx",
                      0x3BCD, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }
  if (_borderReady) {
    renderLayerFromSd(PATH_BORDER_BIN, "border.bin",
                      PATH_BORDER_IDX, "border.idx",
                      0x4A49, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }
  if (_coastReady) {
    renderLayerFromSd(PATH_COAST_BIN, "coast.bin",
                      PATH_COAST_IDX, "coast.idx",
                      TFT_GREEN, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }

  if (_citiesReady) {
    renderCitiesFromSd(z, latMin, latMax, lonMin, lonMax, latRange, lonRange);
  }

  if (hasFix) {
    int gx = (int)((cLon - lonMin) / lonRange * SCREEN_W);
    int gy = (int)((latMax - cLat) / latRange * SCREEN_H);
    cv.drawLine(gx - 6, gy, gx + 6, gy, TFT_RED);
    cv.drawLine(gx, gy - 6, gx, gy + 6, TFT_RED);
    cv.fillCircle(gx, gy, 2, TFT_RED);
  }

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  char zBuf[24];
  snprintf(zBuf, sizeof(zBuf), "z%d [z+/x-]", z);
  cv.setCursor(2, SCREEN_H - 10);
  cv.print(zBuf);

  if (!_indexReady) {
    cv.setTextColor(TFT_ORANGE);
    cv.setCursor(2, 1);
    cv.print("No IDX");
  }
  if (!_citiesReady) {
    cv.setTextColor(TFT_ORANGE);
    cv.setCursor(_indexReady ? 2 : 44, 1);
    cv.print("No CITY");
  }

  if (hasFix) {
    char posBuf[20];
    snprintf(posBuf, sizeof(posBuf), "%.2f,%.2f", cLat, cLon);
    int pw = strlen(posBuf) * 6;
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(SCREEN_W - pw - 2, SCREEN_H - 10);
    cv.print(posBuf);
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(SCREEN_W - 42, SCREEN_H - 10);
    cv.print("No Fix");
  }
}

void FnWorldMap::onUpdate(bool force) {
  drawMap(force);
}

bool FnWorldMap::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;

  bool changed = false;
  if (event.key == 'z' && _zoom < 5) {
    _zoom++;
    changed = true;
  } else if (event.key == 'x' && _zoom > 0) {
    _zoom--;
    changed = true;
  }

  if (changed) {
    drawMap(true);
    DisplayManager::instance().commit();
    return true;
  }

  return false;
}

void FnWorldMap::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 2;
  // Globe
  cv.drawCircle(cx, cy, r, color);
  // Continents hint - small filled regions
  cv.fillCircle(cx - r/3, cy - r/3, 3, color);
  cv.fillCircle(cx + r/4, cy + r/5, 2, color);
  cv.fillCircle(cx - r/4, cy + r/4, 2, color);
  // Equator
  cv.drawLine(cx - r + 3, cy, cx + r - 3, cy, color);
  // Prime meridian
  cv.drawLine(cx, cy - r + 3, cx, cy + r - 3, color);
}
