/**
 * fn_offline_map.cpp - Offline JPEG tile map from SD card.
 */
#include "fn_offline_map.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../sd_manager.h"
#include "../waypoint_manager.h"
#include "../geo_math.h"

#include <math.h>
#include <string.h>

static const uint16_t MAP_BG_COLOR = 0xF77C;
static const uint16_t MAP_TILE_PLACEHOLDER_COLOR = 0x8C51;
static const uint16_t MAP_TILE_PLACEHOLDER_LINE = 0x5AEB;
static const double WEB_MERCATOR_MAX_LAT = 85.05112878;
static const double OFFLINE_MAP_PI = 3.14159265358979323846;
static const double OFFLINE_DEG_TO_RAD = 0.017453292519943295;
static const double OFFLINE_RAD_TO_DEG = 57.29577951308232;

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
  py = (1.0 - asinh(tan(lat * OFFLINE_DEG_TO_RAD)) / OFFLINE_MAP_PI) / 2.0 * worldPx;
}

static void pixelToLatLon(double px, double py, int zoom, double& lat, double& lon) {
  double worldPx = mapWorldPixels(zoom);
  while (px < 0.0) px += worldPx;
  while (px >= worldPx) px -= worldPx;
  if (py < 0.0) py = 0.0;
  if (py > worldPx) py = worldPx;

  lon = px / worldPx * 360.0 - 180.0;
  lat = atan(sinh(OFFLINE_MAP_PI * (1.0 - 2.0 * py / worldPx))) * OFFLINE_RAD_TO_DEG;
  lat = clampMapLat(lat);
}

static int wrapTileX(int x, int zoom) {
  int count = 1 << zoom;
  x %= count;
  if (x < 0) x += count;
  return x;
}

static double wrappedPixelDelta(double nextPx, double prevPx, double worldPx) {
  double delta = fabs(nextPx - prevPx);
  if (delta > worldPx / 2.0) delta = worldPx - delta;
  return delta;
}

void FnOfflineMap::onEnter() {
  _sdReady = SDManager::instance().begin();
  _hasPosition = false;
  _hasGpsPosition = false;
  _followGps = true;
  _userPanned = false;
  _needsRedraw = true;
  _positionDirty = false;
  _lastSaveMs = 0;
  _lastWpFeedbackMs = 0;
  _wpFeedback[0] = '\0';

  if (_sdReady) {
    int maxZoom = SDManager::instance().maxTileZoom();
    _maxZoom = (maxZoom >= ZOOM_MIN && maxZoom <= ZOOM_MAX) ? maxZoom : ZOOM_DEFAULT;
    _zoom = _maxZoom;
  } else {
    _maxZoom = ZOOM_MAX;
    _zoom = ZOOM_DEFAULT;
  }

  if (_sdReady) {
    double lat, lon;
    int savedZoom;
    if (SDManager::instance().loadPosition(lat, lon, savedZoom)) {
      _lat = clampMapLat(lat);
      _lon = wrapMapLon(lon);
      _zoom = constrain(savedZoom, ZOOM_MIN, _maxZoom);
      _hasPosition = true;
      _followGps = false;
    }
  }

  if (!_hasPosition) {
    _centerOnGps();
  }
}

void FnOfflineMap::onExit() {
  if (_sdReady && _hasPosition) {
    SDManager::instance().savePosition(_lat, _lon, _zoom);
    _lastSaveMs = millis();
    _positionDirty = false;
  }
}

bool FnOfflineMap::needsRedraw(unsigned long now) {
  (void)now;
  if (_needsRedraw) return true;

  GPSManager& gps = GPSManager::instance();
  bool hasReliableFix = gps.hasReliableFix();
  if (hasReliableFix != _hasGpsPosition) return true;
  if (!hasReliableFix) return false;

  double nextLat = clampMapLat(gps.latitude());
  double nextLon = wrapMapLon(gps.longitude());
  if (!_hasGpsPosition) return true;

  double prevGpsPx, prevGpsPy, nextGpsPx, nextGpsPy;
  latLonToPixel(_gpsLat, _gpsLon, _zoom, prevGpsPx, prevGpsPy);
  latLonToPixel(nextLat, nextLon, _zoom, nextGpsPx, nextGpsPy);
  double worldPx = mapWorldPixels(_zoom);
  if (wrappedPixelDelta(nextGpsPx, prevGpsPx, worldPx) >= 1.0) return true;
  if (fabs(nextGpsPy - prevGpsPy) >= 1.0) return true;

  return false;
}

void FnOfflineMap::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  if (gps.hasReliableFix()) {
    double nextGpsLat = clampMapLat(gps.latitude());
    double nextGpsLon = wrapMapLon(gps.longitude());
    bool gpsChanged = !_hasGpsPosition || nextGpsLat != _gpsLat || nextGpsLon != _gpsLon;
    _gpsLat = nextGpsLat;
    _gpsLon = nextGpsLon;
    _hasGpsPosition = true;
    if (!_hasPosition) {
      _lat = _gpsLat;
      _lon = _gpsLon;
      _hasPosition = true;
      _followGps = true;
      _needsRedraw = true;
    } else if (_followGps && gpsChanged) {
      _lat = _gpsLat;
      _lon = _gpsLon;
      _needsRedraw = true;
    }
  } else {
    if (_hasGpsPosition) {
      _needsRedraw = true;
    }
    _hasGpsPosition = false;
  }

  if (!force && !_needsRedraw) return;

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

      TileLoadStatus status = TILE_LOAD_NOT_FOUND;
      if (ty >= 0 && ty < tileCount) {
        status = SDManager::instance().loadTile(_zoom, wrapTileX(tx, _zoom), ty, sx, sy);
      }
      if (ty < 0 || ty >= tileCount) {
        int rx = max(sx, 0);
        int ry = max(sy, 0);
        int rw = min(sx + TILE_PX, SCREEN_W) - rx;
        int rh = min(sy + TILE_PX, SCREEN_H) - ry;
        if (rw > 0 && rh > 0) cv.fillRect(rx, ry, rw, rh, MAP_BG_COLOR);
      } else if (status != TILE_LOAD_OK) {
        _drawMissingTilePlaceholder(sx, sy, _zoom, wrapTileX(tx, _zoom), ty, status);
      }

      yield();
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

  // 绘制航点标记
  _drawWaypoints();

  // 航点操作反馈
  if (_wpFeedback[0] != '\0') {
    unsigned long now = millis();
    if (_lastWpFeedbackMs != 0 && now - _lastWpFeedbackMs > 2000) {
      _wpFeedback[0] = '\0';
    } else {
      cv.setTextSize(1);
      cv.setTextColor(TFT_BLACK, TFT_GREEN);
      cv.setCursor(2, 2);
      cv.print(_wpFeedback);
    }
  }

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  char zBuf[32];
  snprintf(zBuf, sizeof(zBuf), "z%d [z+/x-] [w]WP", _zoom);
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

  _needsRedraw = false;
}

bool FnOfflineMap::onKeyEvent(const KeyEvent& event) {
  if (event.pressed) {
    if (event.key == 0x0D) { return true; }
    if (event.key == 'z') { _zoomBy(1); return true; }
    if (event.key == 'x') { _zoomBy(-1); return true; }
    if (event.key == 'g') {
      _centerOnGps();
      if (_hasPosition) {
        _followGps = true;
        _userPanned = false;
        _positionDirty = true;
        _savePositionIfDue(false);
        _needsRedraw = true;
      }
      return true;
    }
    if (event.key == 'w') {
      // w → 从地图中心创建航点
      _createWaypointFromCenter();
      return true;
    }
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
  if (!gps.hasReliableFix()) return;

  _gpsLat = clampMapLat(gps.latitude());
  _gpsLon = wrapMapLon(gps.longitude());
  _lat = _gpsLat;
  _lon = _gpsLon;
  _hasGpsPosition = true;
  _hasPosition = true;
  _followGps = true;
  _needsRedraw = true;
}

void FnOfflineMap::_panByPixels(int dx, int dy) {
  if (!_hasPosition) {
    _centerOnGps();
    if (!_hasPosition) return;
  }

  double px, py;
  latLonToPixel(_lat, _lon, _zoom, px, py);
  pixelToLatLon(px + dx, py + dy, _zoom, _lat, _lon);
  _followGps = false;
  _userPanned = true;
  _positionDirty = true;
  _savePositionIfDue(false);
  _needsRedraw = true;
}

void FnOfflineMap::_zoomBy(int delta) {
  int nextZoom = constrain(_zoom + delta, ZOOM_MIN, _maxZoom);
  if (nextZoom == _zoom) return;
  _zoom = nextZoom;
  _positionDirty = true;
  _savePositionIfDue(false);
  _needsRedraw = true;
}

void FnOfflineMap::_savePositionIfDue(bool force) {
  if (!_sdReady || !_hasPosition || !_positionDirty) return;

  unsigned long now = millis();
  if (!force && _lastSaveMs != 0 && now - _lastSaveMs < MAP_POSITION_SAVE_INTERVAL_MS) {
    return;
  }

  SDManager::instance().savePosition(_lat, _lon, _zoom);
  _lastSaveMs = now;
  _positionDirty = false;
}

void FnOfflineMap::_drawMissingTilePlaceholder(int sx, int sy, int z, int x, int y, TileLoadStatus status) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int rx = max(sx, 0);
  int ry = max(sy, 0);
  int rw = min(sx + TILE_PX, SCREEN_W) - rx;
  int rh = min(sy + TILE_PX, SCREEN_H) - ry;
  if (rw <= 0 || rh <= 0) return;

  cv.fillRect(rx, ry, rw, rh, MAP_TILE_PLACEHOLDER_COLOR);
  cv.drawRect(rx, ry, rw, rh, MAP_TILE_PLACEHOLDER_LINE);
  cv.drawLine(rx, ry, rx + rw - 1, ry + rh - 1, MAP_TILE_PLACEHOLDER_LINE);
  cv.drawLine(rx + rw - 1, ry, rx, ry + rh - 1, MAP_TILE_PLACEHOLDER_LINE);

  if (rw < 48 || rh < 24) return;

  cv.setTextSize(1);
  cv.setTextColor(TFT_WHITE, MAP_TILE_PLACEHOLDER_COLOR);
  cv.setCursor(rx + 4, ry + 4);
  cv.print("No tile");

  if (MAP_SHOW_MISSING_TILE_DEBUG) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d/%d", z, x, y);
    cv.setCursor(rx + 4, ry + 14);
    cv.print(buf);
    snprintf(buf, sizeof(buf), "err:%d", (int)status);
    cv.setCursor(rx + 4, ry + 24);
    cv.print(buf);
  }
}

// ==================================================================
//  航点标记绘制
// ==================================================================
void FnOfflineMap::_drawWaypoints() {
  WaypointManager& wm = WaypointManager::instance();
  if (wm.count() == 0) return;

  M5Canvas& cv = DisplayManager::instance().canvas();

  // 计算当前屏幕的地理范围（用于粗略过滤）
  double centerPx, centerPy;
  latLonToPixel(_lat, _lon, _zoom, centerPx, centerPy);
  double leftPx = centerPx - SCREEN_W / 2.0;
  double topPx = centerPy - SCREEN_H / 2.0;

  double screenLeftLon, screenTopLat, screenRightLon, screenBottomLat;
  pixelToLatLon(leftPx, topPx, _zoom, screenTopLat, screenLeftLon);
  pixelToLatLon(leftPx + SCREEN_W, topPx + SCREEN_H, _zoom, screenBottomLat, screenRightLon);

  // 轻微扩展范围（边界外航点仍可见）
  double pad = (screenRightLon - screenLeftLon) * 0.1;
  double minLon = screenLeftLon - pad;
  double maxLon = screenRightLon + pad;
  double minLat = screenBottomLat - pad;
  double maxLat = screenTopLat + pad;

  double worldPx = mapWorldPixels(_zoom);

  for (size_t i = 0; i < wm.count(); i++) {
    const Waypoint* wp = wm.getByIndex(i);
    if (!wp) continue;

    // 粗略地理范围过滤
    if (wp->lat < minLat || wp->lat > maxLat ||
        wp->lon < minLon || wp->lon > maxLon) continue;

    double wpx, wpy;
    latLonToPixel(wp->lat, wp->lon, _zoom, wpx, wpy);

    // 处理日界线穿越
    double dx = wpx - centerPx;
    if (dx > worldPx / 2.0) dx -= worldPx;
    if (dx < -worldPx / 2.0) dx += worldPx;

    int sx = SCREEN_W / 2 + (int)round(dx);
    int sy = SCREEN_H / 2 + (int)round(wpy - centerPy);

    if (sx < 2 || sx > SCREEN_W - 3 || sy < 2 || sy > SCREEN_H - 3) continue;

    // Cross marker with outline for readability on light/dark maps
    uint16_t wptFill, wptOutline;
    switch ((WaypointType)wp->type) {
      case WP_TYPE_DANGER:    wptFill = TFT_RED;       wptOutline = TFT_BLACK;  break;
      case WP_TYPE_WATER:     wptFill = TFT_BLUE;      wptOutline = TFT_WHITE;  break;
      case WP_TYPE_CAMP:      wptFill = TFT_GREEN;     wptOutline = TFT_BLACK;  break;
      case WP_TYPE_SUMMIT:    wptFill = TFT_ORANGE;    wptOutline = TFT_BLACK;  break;
      case WP_TYPE_VIEWPOINT: wptFill = TFT_YELLOW;    wptOutline = TFT_BLACK;  break;
      case WP_TYPE_TRAILHEAD: wptFill = TFT_MAGENTA;   wptOutline = TFT_BLACK;  break;
      default:                wptFill = TFT_CYAN;      wptOutline = TFT_BLACK;  break;
    }

    // Thick outline cross
    cv.drawLine(sx - 4, sy, sx + 4, sy, wptOutline);
    cv.drawLine(sx, sy - 4, sx, sy + 4, wptOutline);
    // Thin fill cross
    cv.drawLine(sx - 2, sy, sx + 2, sy, wptFill);
    cv.drawLine(sx, sy - 2, sx, sy + 2, wptFill);
    cv.fillCircle(sx, sy, 2, wptFill);

    // Short name (max 3 chars, black bg for readability)
    char shortName[4] = "";
    strncpy(shortName, wp->name, 3);
    shortName[3] = '\0';
    if (strlen(shortName) > 0) {
      cv.setTextSize(1);
      int tw = strlen(shortName) * 6 + 2;
      cv.fillRect(sx + 4, sy - 5, tw, 9, TFT_BLACK);
      cv.setTextColor(wptFill);
      cv.setCursor(sx + 5, sy - 4);
      cv.print(shortName);
    }
  }
}

void FnOfflineMap::_createWaypointFromCenter() {
  WaypointManager& wm = WaypointManager::instance();

  if (wm.isFull()) {
    strncpy(_wpFeedback, "WP list full!", sizeof(_wpFeedback) - 1);
    _wpFeedback[sizeof(_wpFeedback) - 1] = '\0';
    _lastWpFeedbackMs = millis();
    _needsRedraw = true;
    return;
  }

  if (!_hasPosition) {
    strncpy(_wpFeedback, "No map position", sizeof(_wpFeedback) - 1);
    _wpFeedback[sizeof(_wpFeedback) - 1] = '\0';
    _lastWpFeedbackMs = millis();
    _needsRedraw = true;
    return;
  }

  char name[WAYPOINT_NAME_MAX_LEN];
  snprintf(name, sizeof(name), "MAP%03u", wm.nextId());

  // 获取 GPS 时间（如果可用）
  GPSManager& gps = GPSManager::instance();
  int yr = 0, mo = 0, dy = 0, hr = 0, mi = 0, se = 0;
  if (gps.timeValid() && gps.dateValid()) {
    yr = gps.utcYear();
    mo = gps.utcMonth();
    dy = gps.utcDay();
    hr = gps.utcHour();
    mi = gps.utcMinute();
    se = gps.utcSecond();
  }

  const Waypoint* wp = wm.addWaypoint(
    name,
    (float)_lat, (float)_lon, 0.0f,
    WP_SRC_MAP_CENTER,
    WP_TYPE_CUSTOM,
    "",
    yr, mo, dy, hr, mi, se
  );

  if (wp) {
    snprintf(_wpFeedback, sizeof(_wpFeedback), "Saved: %s", wp->name);
  } else {
    snprintf(_wpFeedback, sizeof(_wpFeedback), "Failed: %.12s", wm.lastError());
  }
  _lastWpFeedbackMs = millis();
  _needsRedraw = true;
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
