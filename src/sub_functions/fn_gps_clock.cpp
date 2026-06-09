/**
 * fn_gps_clock.cpp — GPS同步时钟
 * 移植自 GPSInfo SCR_GPS_CLOCK
 */
#include "fn_gps_clock.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../rtc_manager.h"
#include "../imu_manager.h"

void FnGpsClock::onEnter() {}
void FnGpsClock::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  RTCManager& rtc = RTCManager::instance();
  IMUManager& imu = IMUManager::instance();
  char buf[48];

  if (!gps.timeValid()) {
    cv.setTextSize(2);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(30, SCREEN_H / 2 - 12);
    cv.print("No Time Data");
    return;
  }

  // 6行：time(26px) + gap(6) + date(16) + gap(5) + dow(8) + gap(4)
  //        + utc(8) + gap(4) + fix(8) + gap(5) + temp(8) = 98px
  // 均分剩余空间到上下：topPad = (135 - 98) / 2 = 18
  int topPad = (SCREEN_H - 98) / 2;
  if (topPad < 0) topPad = 0;

  // 大号本地时间
  int lh = rtc.localHour(), lm = rtc.localMinute(), ls = rtc.localSecond();
  cv.setTextSize(3);
  cv.setTextColor(TFT_GREEN);
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lh, lm, ls);
  int timeW = strlen(buf) * 18;
  cv.setCursor((SCREEN_W - timeW) / 2, topPad);
  cv.print(buf);

  // 日期
  cv.setTextSize(2);
  cv.setTextColor(TFT_WHITE);
  int dateY = topPad + 26 + 6;
  if (rtc.dateValid()) {
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", rtc.localYear(), rtc.localMonth(), rtc.localDay());
  } else {
    snprintf(buf, sizeof(buf), "----/--/--");
  }
  int dateW = strlen(buf) * 12;
  cv.setCursor((SCREEN_W - dateW) / 2, dateY);
  cv.print(buf);

  // 星期 + UTC偏移
  cv.setTextSize(1);
  cv.setTextColor(TFT_LIGHTGREY);
  int dowY = dateY + 16 + 5;
  const char* days[] = {"Sat","Sun","Mon","Tue","Wed","Thu","Fri"};
  if (rtc.dateValid() && gps.hasFix()) {
    int y2 = rtc.localYear(), m2 = rtc.localMonth(), d2 = rtc.localDay();
    if (m2 < 3) { m2 += 12; y2--; }
    int dow = (d2 + (13*(m2+1))/5 + y2 + y2/4 - y2/100 + y2/400) % 7;
    snprintf(buf, sizeof(buf), "%s  UTC%+d%s", days[dow], rtc.timezoneOffset(),
             (rtc.timezoneOffset() != (int)roundf(gps.longitude()/15.0f)) ? " DST" : "");
  } else {
    snprintf(buf, sizeof(buf), "UTC%+d", rtc.timezoneOffset());
  }
  int dowW = strlen(buf) * 6;
  cv.setCursor((SCREEN_W - dowW) / 2, dowY);
  cv.print(buf);

  // UTC参考时间
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  int utcY = dowY + 8 + 4;
  snprintf(buf, sizeof(buf), "UTC %02d:%02d:%02d", gps.utcHour(), gps.utcMinute(), gps.utcSecond());
  int utcW = strlen(buf) * 6;
  cv.setCursor((SCREEN_W - utcW) / 2, utcY);
  cv.print(buf);

  // GPS fix状态
  int fixY = utcY + 8 + 4;
  const char* fixStr = (gps.ggaFixQuality() >= 1) ? "Fix OK" : "No Fix";
  uint16_t fixCol = (gps.ggaFixQuality() >= 1) ? TFT_GREEN : TFT_RED;
  cv.setTextColor(fixCol);
  snprintf(buf, sizeof(buf), "GPS: %s", fixStr);
  int fixW = strlen(buf) * 6;
  cv.setCursor((SCREEN_W - fixW) / 2, fixY);
  cv.print(buf);

  // 温度（IMU）
  if (imu.isAvailable()) {
    cv.setTextColor(TFT_DARKGREY);
    int tmpY = fixY + 8 + 5;
    snprintf(buf, sizeof(buf), "%.1fC", imu.temperature());
    int tmpW = strlen(buf) * 6;
    cv.setCursor((SCREEN_W - tmpW) / 2, tmpY);
    cv.print(buf);
  }
}

void FnGpsClock::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2, r = size / 2 - 2;
  // Outer bezel
  cv.drawCircle(cx, cy, r, color);
  cv.drawCircle(cx, cy, r - 1, color);
  // Hour ticks
  for (int a = 0; a < 360; a += 30) {
    float rad = radians(a);
    int tx = cx + (r - 3) * sin(rad), ty = cy - (r - 3) * cos(rad);
    int ix = cx + (r - 6) * sin(rad), iy = cy - (r - 6) * cos(rad);
    cv.drawLine(tx, ty, ix, iy, color);
  }
  // Hour + minute hands
  cv.drawLine(cx, cy, cx, cy - r + 7, color);
  cv.drawLine(cx, cy, cx + r - 7, cy, color);
  // Center pin
  cv.fillCircle(cx, cy, 2, color);
}
