/**
 * fn_waypoint.cpp — 航点导航（指南针+方位/距离）
 * 移植自 GPSInfo SCR_WAYPOINT
 */
#include "fn_waypoint.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../ui_helpers.h"

void FnWaypoint::onEnter() { _waypointSet = false; _editing = false; }
void FnWaypoint::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  char buf[48];

  if (_editing) {
    // === 航点编辑模式 ===
    cv.setTextSize(1);
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(40, CONTENT_TOP + 4);
    cv.print("-- Enter Waypoint --");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(8, CONTENT_TOP + 18);
    cv.print("[;]Switch [0-9. -] [Enter]Save");

    cv.setTextSize(2);
    int yLat = CONTENT_TOP + 38, yLon = CONTENT_TOP + 68;
    cv.setTextColor(_field == 0 ? TFT_GREEN : TFT_WHITE);
    cv.setCursor(10, yLat);
    cv.printf("Lat: %s", _tmp[0].c_str());
    if (_field == 0) cv.print("_");
    cv.setTextColor(_field == 1 ? TFT_GREEN : TFT_WHITE);
    cv.setCursor(10, yLon);
    cv.printf("Lon: %s", _tmp[1].c_str());
    if (_field == 1) cv.print("_");

    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(10, CONTENT_TOP + 96);
    cv.print("Example: 40.689247 / -74.044502");
    cv.setCursor(10, CONTENT_TOP + 110);
    cv.print("[w] cancel  [DEL] backspace");
    return;
  }

  if (!_waypointSet) {
    // === 无航点 ===
    cv.setTextSize(1);
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(40, CONTENT_TOP + 4);
    cv.print("-- Waypoint Nav --");
    cv.setTextSize(2);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(30, CONTENT_TOP + 40);
    cv.print("No waypoint");
    cv.setTextSize(1);
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(35, CONTENT_TOP + 75);
    cv.print("Press [w] to set target");
    return;
  }

  // === 导航模式 ===
  if (!gps.hasFix()) {
    cv.setTextColor(TFT_RED);
    cv.setCursor(60, CONTENT_TOP + 55);
    cv.print("No GPS fix");
    return;
  }

  float curLat = gps.latitude(), curLon = gps.longitude();
  float bearing = bearingTo(curLat, curLon, _waypointLat, _waypointLon);
  float distKm = haversineKm(curLat, curLon, _waypointLat, _waypointLon);
  bool courseValid = gps.courseDeg() > 0 && gps.speedKmph() > 1.0f;

  int cx = 68, cy = CONTENT_TOP + CONTENT_H / 2 + 5, cr = 48;
  float relBearing = bearing;
  if (courseValid) relBearing = fmod(bearing - gps.courseDeg() + 360.0f, 360.0f);

  // 罗盘环
  cv.drawCircle(cx, cy, cr, TFT_DARKGREY);
  cv.drawCircle(cx, cy, cr + 1, TFT_DARKGREY);
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(cx - 2, cy - cr - 9); cv.print("N");
  cv.setCursor(cx - 2, cy + cr + 2); cv.print("S");
  cv.setCursor(cx + cr + 3, cy - 3); cv.print("E");
  cv.setCursor(cx - cr - 8, cy - 3); cv.print("W");

  // 箭头
  float ang = (courseValid ? relBearing : bearing) * DEG_TO_RAD;
  float tipR = cr - 6, tailR = 14, wingR = 12;
  int tipX = cx + (int)(sin(ang) * tipR);
  int tipY = cy - (int)(cos(ang) * tipR);
  int tailX = cx - (int)(sin(ang) * tailR);
  int tailY = cy + (int)(cos(ang) * tailR);
  int lwX = cx - (int)(sin(ang - 0.4) * wingR);
  int lwY = cy + (int)(cos(ang - 0.4) * wingR);
  int rwX = cx - (int)(sin(ang + 0.4) * wingR);
  int rwY = cy + (int)(cos(ang + 0.4) * wingR);
  cv.fillTriangle(tipX, tipY, lwX, lwY, rwX, rwY, TFT_GREEN);
  cv.drawLine(cx, cy, tailX, tailY, TFT_DARKGREY);
  if (courseValid) cv.fillCircle(cx, cy - cr + 3, 2, TFT_GREEN);

  // 右侧信息面板
  int rx = 136;
  cv.setTextSize(1);
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(rx, CONTENT_TOP + 4); cv.print("DISTANCE");
  cv.setTextSize(2);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(rx, CONTENT_TOP + 14);
  if (distKm < 1.0) snprintf(buf, sizeof(buf), "%.0fm", distKm * 1000.0);
  else if (distKm < 100.0) snprintf(buf, sizeof(buf), "%.2fkm", distKm);
  else snprintf(buf, sizeof(buf), "%.1fkm", distKm);
  cv.print(buf);

  cv.setTextSize(1);
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(rx, CONTENT_TOP + 38); cv.print("BEARING");
  cv.setTextSize(2);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(rx, CONTENT_TOP + 48);
  snprintf(buf, sizeof(buf), "%.0f%s", bearing, cardinalFromHeading(bearing));
  cv.print(buf);

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(rx, CONTENT_TOP + 72);
  snprintf(buf, sizeof(buf), "%.5f", _waypointLat); cv.print(buf);
  cv.setCursor(rx, CONTENT_TOP + 82);
  snprintf(buf, sizeof(buf), "%.5f", _waypointLon); cv.print(buf);

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(rx, CONTENT_TOP + 98);
  cv.print(courseValid ? "Mode: Track up" : "Mode: North up");
  cv.setCursor(rx, CONTENT_TOP + 112);
  cv.print("[w] edit [r] clear");
}

bool FnWaypoint::onKeyEvent(const KeyEvent& event) {
  if (_editing) {
    // 编辑模式按键
    if (!event.pressed) return false;
    if (event.key == ';') { _field = (_field + 1) % 2; return true; }
    if (event.key >= '0' && event.key <= '9') { _tmp[_field] += event.key; return true; }
    if (event.key == '.' && _tmp[_field].indexOf('.') == -1) { _tmp[_field] += '.'; return true; }
    if (event.key == '-' && _tmp[_field].length() == 0) { _tmp[_field] += '-'; return true; }
    if (event.key == 0x7F) { /* DEL */ if (_tmp[_field].length() > 0) _tmp[_field].remove(_tmp[_field].length() - 1); return true; }
    if (event.key == 0x0D) { /* Enter */
      if (_tmp[0].length() > 0 && _tmp[1].length() > 0) {
        _waypointLat = _tmp[0].toFloat();
        _waypointLon = _tmp[1].toFloat();
        _waypointSet = true;
      }
      _tmp[0] = _tmp[1] = "";
      _field = 0; _editing = false;
      return true;
    }
    if (event.key == 'w') { _tmp[0] = _tmp[1] = ""; _field = 0; _editing = false; return true; }
    return false;
  }

  // 非编辑模式
  if (event.pressed) {
    if (event.key == 'w') { _editing = true; _field = 0; _tmp[0] = _tmp[1] = ""; return true; }
    if (event.key == 'r') { _waypointSet = false; return true; }
  }
  return false;
}

void FnWaypoint::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2;
  // Pin shape
  cv.fillCircle(cx, cy - size/5, size/3, color);
  cv.fillTriangle(cx - 4, cy - size/5 + 2, cx + 4, cy - size/5 + 2, cx, cy + size/3, color);
  // Inner dot
  cv.fillCircle(cx, cy - size/5, 2, TFT_BLACK);
}
