/**
 * fn_offline_map.cpp - Offline JPEG tile map from SD card.
 */
#include "fn_offline_map.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../sd_manager.h"

#include <math.h>
#include <string.h>

static const uint16_t MAP_BG_COLOR = 0xF77C;
static const double WEB_MERCATOR_MAX_LAT = 85.05112878;
static const double MAP_PI = 3.14159265358979323846;
static const double DEG_TO_RAD_D = 0.017453292519943295;
static const double RAD_TO_DEG_D = 57.29577951308232;

static double clampMapLat(double lat) {
  if (lat > WEB_MERCATOR_MAX_LAT) return WEB_MERCATOR_MAX_LAT;
  if (lat < -WEB_MERCATOR_MAX_LAT) return -WEB_MERCATOR_MAX_LAT;
  return lat;
}

static double wrapMapLon(double lon) {
  while (lon < -180.0) lon += 360.0;
  while (lon >= 180.0) lon -= 360.0;
  return lon;
}

static double mapWorldPixels(int zoom) {
  return (double)(1 << zoom) * TILE_PX;
}

static void latLonToPixel(double lat, double lon, int zoom, double& px, double& py) {
  lat = clampMapLat(lat);
  double worldPx = mapWorldPixels(zoom);
  px = (wrapMapLon(lon) + 180.0) / 360.0 * worldPx;
  py = (1.0 - asinh(tan(lat * DEG_TO_RAD_D)) / MAP_PI) / 2.0 * worldPx;
}

static void pixelToLatLon(double px, double py, int zoom, double& lat, double& lon) {
  double worldPx = mapWorldPixels(zoom);
  while (px < 0.0) px += worldPx;
  while (px >= worldPx) px -= worldPx;
  if (py < 0.0) py = 0.0;
  if (py > worldPx) py = worldPx;

  lon = px / worldPx * 360.0 - 180.0;
  lat = atan(sinh(MAP_PI * (1.0 - 2.0 * py / worldPx))) * RAD_TO_DEG_D;
  lat = clampMapLat(lat);
}

static int wrapTileX(int x, int zoom) {
  int count = 1 << zoom;
  x %= count;
  if (x < 0) x += count;
  return x;
}

void FnOfflineMap::onEnter() {
  _sdReady = SDManager::instance().begin();
  _hasPosition = false;
  _hasGpsPosition = false;
  _centeredOnGps = false;
  _userPanned = false;

  if (_sdReady) {
    int maxZoom = SDManager::instance().maxTileZoom();
    _zoom = (maxZoom >= ZOOM_MIN && maxZoom <= ZOOM_MAX) ? maxZoom : ZOOM_DEFAULT;
  } else {
    _zoom = ZOOM_DEFAULT;
  }

  _centerOnGps();

  if (!_hasPosition && _sdReady) {
    double lat, lon;
    int savedZoom;
    if (SDManager::instance().loadPosition(lat, lon, savedZoom)) {
      (void)savedZoom;
      _lat = clampMapLat(lat);
      _lon = wrapMapLon(lon);
      _hasPosition = true;
    }
  }
}

void FnOfflineMap::onUpdate(bool force) {
  (void)force;

  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  if (gps.hasFix()) {
    _gpsLat = clampMapLat(gps.latitude());
    _gpsLon = wrapMapLon(gps.longitude());
    _hasGpsPosition = true;
    if (!_hasPosition || (!_centeredOnGps && !_userPanned)) {
      _lat = _gpsLat;
      _lon = _gpsLon;
      _hasPosition = true;
      _centeredOnGps = true;
    }
  }

  cv.fillScreen(MAP_BG_COLOR);

  if (!_sdReady) {
    cv.fillScreen(TFT_BLACK);
    cv.setTextSize(1);
    cv.setTextColor(TFT_RED);
    cv.setCursor(30, SCREEN_H / 2 - 4);
    cv.print("SD Card not available");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(35, SCREEN_H / 2 + 10);
    cv.print("Insert SD with /gpstoolkit tiles");
    return;
  }

  if (!_hasPosition) {
    cv.fillScreen(TFT_BLACK);
    cv.setTextSize(1);
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(SCREEN_W / 2 - 48, SCREEN_H / 2 - 4);
    cv.print("Waiting for GPS...");
    return;
  }

  double centerPx, centerPy;
  latLonToPixel(_lat, _lon, _zoom, centerPx, centerPy);

  double leftPx = centerPx - SCREEN_W / 2.0;
  double topPx = centerPy - SCREEN_H / 2.0;
  int firstTileX = (int)floor(leftPx / TILE_PX);
  int firstTileY = (int)floor(topPx / TILE_PX);
  int lastTileX = (int)floor((leftPx + SCREEN_W - 1) / TILE_PX);
  int lastTileY = (int)floor((topPx + SCREEN_H - 1) / TILE_PX);
  int tileCount = 1 << _zoom;

  for (int tx = firstTileX; tx <= lastTileX; tx++) {
    for (int ty = firstTileY; ty <= lastTileY; ty++) {
      int sx = (int)floor(tx * TILE_PX - leftPx);
      int sy = (int)floor(ty * TILE_PX - topPx);
      if (sx + TILE_PX <= 0 || sx >= SCREEN_W) continue;
      if (sy + TILE_PX <= 0 || sy >= SCREEN_H) continue;

      bool ok = false;
      if (ty >= 0 && ty < tileCount) {
        ok = SDManager::instance().loadTile(_zoom, wrapTileX(tx, _zoom), ty, sx, sy);
      }
      if (!ok) {
        int rx = max(sx, 0);
        int ry = max(sy, 0);
        int rw = min(sx + TILE_PX, SCREEN_W) - rx;
        int rh = min(sy + TILE_PX, SCREEN_H) - ry;
        if (rw > 0 && rh > 0) cv.fillRect(rx, ry, rw, rh, MAP_BG_COLOR);
      }
    }
  }

  if (_hasGpsPosition) {
    double gpsPx, gpsPy;
    latLonToPixel(_gpsLat, _gpsLon, _zoom, gpsPx, gpsPy);
    double worldPx = mapWorldPixels(_zoom);
    double dx = gpsPx - centerPx;
    if (dx > worldPx / 2.0) dx -= worldPx;
    if (dx < -worldPx / 2.0) dx += worldPx;
    int mx = SCREEN_W / 2 + (int)round(dx);
    int my = SCREEN_H / 2 + (int)round(gpsPy - centerPy);
    if (mx >= 0 && mx < SCREEN_W && my >= 0 && my < SCREEN_H) {
      cv.fillCircle(mx, my, 4, TFT_WHITE);
      cv.fillCircle(mx, my, 2, TFT_RED);
    }
  }

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  char zBuf[24];
  snprintf(zBuf, sizeof(zBuf), "z%d [z+/x-]", _zoom);
  cv.setCursor(2, SCREEN_H - 10);
  cv.print(zBuf);

  if (_hasGpsPosition) {
    char posBuf[20];
    snprintf(posBuf, sizeof(posBuf), "%.2f,%.2f", _gpsLat, _gpsLon);
    int pw = strlen(posBuf) * 6;
    cv.setCursor(SCREEN_W - pw - 2, SCREEN_H - 10);
    cv.print(posBuf);
  } else {
    cv.setCursor(SCREEN_W - 42, SCREEN_H - 10);
    cv.print("No Fix");
  }
}

bool FnOfflineMap::onKeyEvent(const KeyEvent& event) {
  if (event.pressed) {
    if (event.key == 'z') { _zoomBy(1); return true; }
    if (event.key == 'x') { _zoomBy(-1); return true; }
  }

  if (event.pressed || event.held) {
    switch (event.key) {
      case ';': _panByPixels(0, -PAN_STEP); return true;
      case '.': _panByPixels(0, PAN_STEP); return true;
      case ',': _panByPixels(-PAN_STEP, 0); return true;
      case '/': _panByPixels(PAN_STEP, 0); return true;
    }
  }
  return false;
}

void FnOfflineMap::_centerOnGps() {
  GPSManager& gps = GPSManager::instance();
  if (!gps.hasFix()) return;

  _gpsLat = clampMapLat(gps.latitude());
  _gpsLon = wrapMapLon(gps.longitude());
  _lat = _gpsLat;
  _lon = _gpsLon;
  _hasGpsPosition = true;
  _hasPosition = true;
  _centeredOnGps = true;
}

void FnOfflineMap::_panByPixels(int dx, int dy) {
  if (!_hasPosition) {
    _centerOnGps();
    if (!_hasPosition) return;
  }

  double px, py;
  latLonToPixel(_lat, _lon, _zoom, px, py);
  pixelToLatLon(px + dx, py + dy, _zoom, _lat, _lon);
  _userPanned = true;
  onUpdate(true);
  DisplayManager::instance().commit();
}

void FnOfflineMap::_zoomBy(int delta) {
  int nextZoom = constrain(_zoom + delta, ZOOM_MIN, ZOOM_MAX);
  if (nextZoom == _zoom) return;
  _zoom = nextZoom;
  onUpdate(true);
  DisplayManager::instance().commit();
}

void FnOfflineMap::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int gap = 3, half = (size - gap) / 2;
  cv.drawRect(x, y, half, half, color);
  cv.drawRect(x + half + gap, y, half, half, color);
  cv.drawRect(x, y + half + gap, half, half, color);
  cv.drawRect(x + half + gap, y + half + gap, half, half, color);
  int cx = x + size / 2, cy = y + size / 2;
  cv.fillCircle(cx, cy - 2, 3, color);
  cv.fillTriangle(cx - 2, cy + 1, cx + 2, cy + 1, cx, cy + 5, color);
}
