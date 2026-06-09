/**
 * fn_offline_map.cpp — 离线瓦片地图（SD卡JPEG瓦片）
 * 移植自 GPSMap SCR_MAP
 */
#include "fn_offline_map.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../sd_manager.h"
#include "../rtc_manager.h"

void FnOfflineMap::onEnter() {
  _sdReady = SDManager::instance().begin();
  // 从GPS获取初始位置
  GPSManager& gps = GPSManager::instance();
  if (gps.hasFix()) { _lat = gps.latitude(); _lon = gps.longitude(); }
}

void FnOfflineMap::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  cv.fillScreen(TFT_BLACK);

  if (!_sdReady) {
    cv.setTextSize(1);
    cv.setTextColor(TFT_RED);
    cv.setCursor(30, SCREEN_H / 2 - 4);
    cv.print("SD Card not available");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(35, SCREEN_H / 2 + 10);
    cv.print("Insert SD with /gpsmap tiles");
    return;
  }

  // 绘制3x3瓦片网格
  double n = (double)(1 << _zoom);
  double gpx = (_lon + 180.0) / 360.0 * n * TILE_PX;
  double gpy = (1.0 - asinh(tan(_lat * DEG_TO_RAD)) / M_PI) / 2.0 * n * TILE_PX;
  int tx = (int)((_lon + 180.0) / 360.0 * n);
  int ty = (int)((1.0 - asinh(tan(_lat * DEG_TO_RAD)) / M_PI) / 2.0 * n);

  double inTileX = gpx - tx * TILE_PX + _panX;
  double inTileY = gpy - ty * TILE_PX + _panY;
  int baseX = SCREEN_W / 2 - (int)inTileX;
  int baseY = SCREEN_H / 2 - (int)inTileY;

  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      int sx = baseX + dx * TILE_PX;
      int sy = baseY + dy * TILE_PX;
      if (sx + TILE_PX < 0 || sx > SCREEN_W) continue;
      if (sy + TILE_PX < 0 || sy > SCREEN_H) continue;
      if (!SDManager::instance().loadTile(_zoom, tx + dx, ty + dy, sx, sy)) {
        int rx = (sx > 0) ? sx : 0;
        int ry = (sy > 0) ? sy : 0;
        int rw = ((TILE_PX < SCREEN_W - rx) ? TILE_PX : SCREEN_W - rx);
        int rh = ((TILE_PX < SCREEN_H - ry) ? TILE_PX : SCREEN_H - ry);
        if (rw > 0 && rh > 0) cv.fillRect(rx, ry, rw, rh, 0xF77C);
      }
    }
  }

  // GPS位置标记
  int mx = SCREEN_W / 2 - (int)_panX;
  int my = SCREEN_H / 2 - (int)_panY;
  if (mx >= 0 && mx < SCREEN_W && my >= 0 && my < SCREEN_H) {
    cv.fillCircle(mx, my, 4, TFT_WHITE);
    cv.fillCircle(mx, my, 2, TFT_RED);
  }

  // 顶部信息栏
  cv.fillRect(0, 0, SCREEN_W, 12, TFT_BLACK);
  cv.setTextSize(1);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(3, 2);
  if (gps.hasFix()) {
    char buf[30];
    snprintf(buf, sizeof(buf), "Z:%d %.5f %.5f", _zoom, _lat, _lon);
    cv.print(buf);
  } else {
    cv.printf("Z:%d [no fix]", _zoom);
  }

  // 右上角时间
  RTCManager& rtc = RTCManager::instance();
  if (rtc.isSynced()) {
    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d", rtc.localHour(), rtc.localMinute());
    cv.setCursor(SCREEN_W - cv.textWidth(tbuf) - 4, 2);
    cv.print(tbuf);
  }

  if (!gps.hasFix()) {
    cv.fillRect(0, SCREEN_H - 12, SCREEN_W, 12, TFT_BLACK);
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(SCREEN_W / 2 - 50, SCREEN_H - 11);
    cv.print("Searching GPS...");
  }
}

bool FnOfflineMap::onKeyEvent(const KeyEvent& event) {
  if (event.pressed) {
    if (event.key == 'z' && _zoom > ZOOM_MIN) { _zoom--; return true; }
    if (event.key == 'x' && _zoom < ZOOM_MAX) { _zoom++; return true; }
  }
  // 方向键平移（press和held都响应）
  if (event.pressed || event.held) {
    switch (event.key) {
      case ';': _panY -= PAN_STEP; return true;
      case '.': _panY += PAN_STEP; return true;
      case ',': _panX -= PAN_STEP; return true;
      case '/': _panX += PAN_STEP; return true;
    }
  }
  return false;
}

void FnOfflineMap::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int gap = 3, half = (size - gap) / 2;
  // 4 tile squares
  cv.drawRect(x, y, half, half, color);
  cv.drawRect(x + half + gap, y, half, half, color);
  cv.drawRect(x, y + half + gap, half, half, color);
  cv.drawRect(x + half + gap, y + half + gap, half, half, color);
  // Map pin in center
  int cx = x + size / 2, cy = y + size / 2;
  cv.fillCircle(cx, cy - 2, 3, color);
  cv.fillTriangle(cx - 2, cy + 1, cx + 2, cy + 1, cx, cy + 5, color);
}
