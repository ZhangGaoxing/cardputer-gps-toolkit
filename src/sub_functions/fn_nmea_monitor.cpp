/**
 * fn_nmea_monitor.cpp — NMEA监视器
 * 移植自 GPSInfo SCR_NMEA_MONITOR
 */
#include "fn_nmea_monitor.h"
#include "../display_manager.h"
#include "../gps_manager.h"

void FnNmeaMonitor::onEnter() {}
void FnNmeaMonitor::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  cv.setTextSize(1);
  cv.setTextColor(TFT_GREEN, TFT_BLACK);
  int y = CONTENT_TOP + 1;
  int nneaCount = gps.nmeaLineCount();

  if (nneaCount == 0) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(30, CONTENT_TOP + 55);
    cv.print("Waiting for NMEA data...");
    return;
  }

  int maxLines = (nneaCount < 16) ? nneaCount : 16;
  for (int i = 0; i < maxLines; i++) {
    const char* line = gps.nmeaLine(nneaCount - maxLines + i);
    cv.setCursor(2, y);
    // 截断到40字符（240px / 6px）
    char truncated[41];
    strncpy(truncated, line, 40);
    truncated[40] = '\0';
    cv.print(truncated);
    y += 8;
  }
}

void FnNmeaMonitor::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  // Text lines icon
  for (int i = 0; i < 5; i++) {
    int ly = y + 3 + i * (size / 5 + 1);
    int lw = (i == 2) ? size * 3 / 4 : size - 2;
    cv.drawLine(x + 2, ly, x + 2 + lw, ly, color);
  }
  // Cursor block on line 2
  int cy = y + 3 + 2 * (size / 5 + 1);
  cv.fillRect(x + size / 2, cy - 1, 4, 2, color);
}
