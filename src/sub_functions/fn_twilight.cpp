/**
 * fn_twilight.cpp — 晨昏蒙影与日月出没
 *
 * 页面风格与其他子功能保持一致：
 *   - 标题行 (y=2, TFT_DARKGREY, size=1) + "[Tab]" 右侧提示
 *   - 内容区从 y=12 开始
 *   - 单条分隔线区隔日出/日落区与蒙影区
 *   - 底部提示栏 (y=SCREEN_H-12) 与 peers / trip 风格一致
 */
#include "fn_twilight.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../rtc_manager.h"
#include <math.h>

// ================================================================
//  常量
// ================================================================
static const double TW_D2R = M_PI / 180.0;
static const double TW_R2D = 180.0 / M_PI;

// ================================================================
//  格式化辅助
// ================================================================

void FnTwilight::_fmtLocal(int utcMin, int tzOff, char* buf, int len) {
  int local = utcMin + tzOff * 60;
  local = ((local % 1440) + 1440) % 1440;
  snprintf(buf, len, "%02d:%02d", local / 60, local % 60);
}

void FnTwilight::_fmtDuration(int minutes, char* buf, int len) {
  if (minutes < 0) minutes = 0;
  int h = minutes / 60;
  int m = minutes % 60;
  snprintf(buf, len, "%dh%02dm", h, m);
}

// ================================================================
//  儒略日计算
// ================================================================

double FnTwilight::_calcJD(int y, int m, int d) {
  if (m <= 2) { y -= 1; m += 12; }
  int A = y / 100;
  int B = 2 - A + A / 4;
  return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

// ================================================================
//  NOAA 太阳出没算法
//  zenithDeg: 90.833=日出/日落, 96=民用, 102=航海, 108=天文
//  返回 UTC 分钟（自当日 0:00 起）
// ================================================================

void FnTwilight::_calcSunEvent(double jd, double latDeg, double lonDeg,
                                double zenithDeg,
                                int* riseMin, int* setMin,
                                bool* alwaysUp, bool* alwaysDown) {
  *riseMin   = -1;
  *setMin    = -1;
  *alwaysUp  = false;
  *alwaysDown = false;

  double T  = (jd - 2451545.0) / 36525.0;
  double L0 = fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
  double M  = fmod(357.52911 + T * (35999.05029 - 0.0001537 * T), 360.0);
  double Mrad = M * TW_D2R;
  double e  = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);

  double C  = sin(Mrad)     * (1.914602 - T * (0.004817 + 0.000014 * T))
            + sin(2*Mrad)   * (0.019993 - 0.000101 * T)
            + sin(3*Mrad)   * 0.000289;

  double sunAppLon = (L0 + C) - 0.00569
                   - 0.00478 * sin((125.04 - 1934.136 * T) * TW_D2R);

  double obliq     = 23.0 + 26.0/60.0 + 21.448/3600.0
                   - T * (46.815/3600.0 + T * (0.00059/3600.0 - T * 0.001813/3600.0));
  double obliqCorr = obliq + 0.00256 * cos((125.04 - 1934.136 * T) * TW_D2R);

  double declin = asin(sin(obliqCorr * TW_D2R) * sin(sunAppLon * TW_D2R)) * TW_R2D;

  double yy = tan(obliqCorr / 2.0 * TW_D2R);
  yy *= yy;
  double eqTime = 4.0 * TW_R2D * (
      yy * sin(2 * L0 * TW_D2R)
    - 2 * e * sin(M * TW_D2R)
    + 4 * e * yy * sin(M * TW_D2R) * cos(2 * L0 * TW_D2R)
    - 0.5 * yy * yy * sin(4 * L0 * TW_D2R)
    - 1.25 * e * e * sin(2 * M * TW_D2R));

  double solarNoon = 720.0 - 4.0 * lonDeg - eqTime;

  double cosHA = (cos(zenithDeg * TW_D2R)
                - sin(latDeg * TW_D2R) * sin(declin * TW_D2R))
               / (cos(latDeg * TW_D2R) * cos(declin * TW_D2R));

  if (cosHA > 1.0)  { *alwaysDown = true; return; }
  if (cosHA < -1.0) { *alwaysUp   = true; return; }

  double HA = acos(cosHA) * TW_R2D;
  *riseMin = (int)round(solarNoon - HA * 4.0);
  *setMin  = (int)round(solarNoon + HA * 4.0);
}

// ================================================================
//  计算全部太阳时刻
// ================================================================

FnTwilight::SunTimes FnTwilight::_calcAllSunTimes(double jd, double latDeg, double lonDeg) {
  SunTimes st;
  memset(&st, 0, sizeof(st));
  st.sunriseMin = st.sunsetMin = -1;
  st.civilDawnMin = st.civilDuskMin = -1;
  st.nautDawnMin  = st.nautDuskMin  = -1;
  st.astroDawnMin = st.astroDuskMin = -1;

  bool u, d;
  _calcSunEvent(jd, latDeg, lonDeg, 90.833, &st.sunriseMin,  &st.sunsetMin,  &st.alwaysUp, &st.alwaysDown);
  _calcSunEvent(jd, latDeg, lonDeg, 96.0,   &st.civilDawnMin,&st.civilDuskMin, &u, &d);
  _calcSunEvent(jd, latDeg, lonDeg, 102.0,  &st.nautDawnMin, &st.nautDuskMin,  &u, &d);
  _calcSunEvent(jd, latDeg, lonDeg, 108.0,  &st.astroDawnMin,&st.astroDuskMin, &u, &d);

  // 正午（重用方程式，不依赖时角）
  double T  = (jd - 2451545.0) / 36525.0;
  double L0 = fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
  double M  = fmod(357.52911 + T * (35999.05029 - 0.0001537 * T), 360.0);
  double Mrad = M * TW_D2R;
  double e  = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
  double C  = sin(Mrad)   * (1.914602 - T * (0.004817 + 0.000014 * T))
            + sin(2*Mrad) * (0.019993 - 0.000101 * T)
            + sin(3*Mrad) * 0.000289;
  double sunAppLon = (L0 + C) - 0.00569 - 0.00478 * sin((125.04 - 1934.136 * T) * TW_D2R);
  double obliq     = 23.0 + 26.0/60.0 + 21.448/3600.0
                   - T * (46.815/3600.0 + T * 0.00059/3600.0);
  double obliqCorr = obliq + 0.00256 * cos((125.04 - 1934.136 * T) * TW_D2R);
  double yy = tan(obliqCorr / 2.0 * TW_D2R);
  yy *= yy;
  double eqTime = 4.0 * TW_R2D * (
      yy * sin(2 * L0 * TW_D2R)
    - 2 * e * sin(M * TW_D2R)
    + 4 * e * yy * sin(M * TW_D2R) * cos(2 * L0 * TW_D2R)
    - 0.5 * yy * yy * sin(4 * L0 * TW_D2R)
    - 1.25 * e * e * sin(2 * M * TW_D2R));
  st.solarNoonMin = (int)round(720.0 - 4.0 * lonDeg - eqTime);
  return st;
}

// ================================================================
//  月球高度角（Jean Meeus 简化版，精度 ~1°）
// ================================================================

double FnTwilight::_moonAltitude(double jd, double latDeg, double lonDeg) {
  double T  = (jd - 2451545.0) / 36525.0;

  double Lp = fmod(218.3164591 + 481267.88134236 * T - 0.0013268 * T * T, 360.0);
  double M  = fmod(357.5291092 +  35999.0502909  * T - 0.0001536 * T * T, 360.0);
  double Mp = fmod(134.9634114 + 477198.8676313  * T + 0.0089970 * T * T, 360.0);
  double D  = fmod(297.8502042 + 445267.1115168  * T - 0.0016300 * T * T, 360.0);
  double F  = fmod(93.2720993  + 483202.0175273  * T - 0.0034029 * T * T, 360.0);

  double Mrad  = M  * TW_D2R;
  double Mprad = Mp * TW_D2R;
  double Drad  = D  * TW_D2R;
  double Frad  = F  * TW_D2R;

  // 黄经修正（度）
  double dL = -1.274 * sin(Mprad - 2*Drad)
             + 0.658 * sin(2*Drad)
             - 0.186 * sin(Mrad)
             - 0.059 * sin(2*Mprad - 2*Drad)
             - 0.057 * sin(Mprad - 2*Drad + Mrad)
             + 0.053 * sin(Mprad + 2*Drad)
             + 0.046 * sin(2*Drad - Mrad)
             + 0.041 * sin(Mprad - Mrad)
             - 0.035 * sin(Drad)
             - 0.031 * sin(Mprad + Mrad)
             - 0.015 * sin(2*Frad - 2*Drad)
             + 0.011 * sin(Mprad - 4*Drad);

  // 黄纬修正（度）
  double dB = -0.173 * sin(Frad - 2*Drad)
              - 0.055 * sin(Mprad - Frad - 2*Drad)
              - 0.046 * sin(Mprad + Frad - 2*Drad)
              + 0.033 * sin(Frad + 2*Drad)
              + 0.017 * sin(2*Mprad + Frad);

  double moonLon = fmod(Lp + dL + 360.0, 360.0) * TW_D2R;
  double moonLat = dB * TW_D2R;

  // 黄赤交角（简化）
  double oblR = (23.4392911 - 0.013004167 * T) * TW_D2R;

  // 黄道坐标 → 赤道坐标
  double ra  = atan2(sin(moonLon) * cos(oblR) - tan(moonLat) * sin(oblR), cos(moonLon));
  double dec = asin(sin(moonLat) * cos(oblR) + cos(moonLat) * sin(oblR) * sin(moonLon));

  // 格林威治恒星时（弧度）
  double gst = fmod(280.46061837 + 360.98564736629 * (jd - 2451545.0)
                   + 0.000387933 * T * T, 360.0) * TW_D2R;

  // 时角（弧度）
  double H   = gst + lonDeg * TW_D2R - ra;
  double latR = latDeg * TW_D2R;

  // 高度角（弧度 → 度）
  return asin(sin(latR) * sin(dec) + cos(latR) * cos(dec) * cos(H)) * TW_R2D;
}

// ================================================================
//  月出/月落 & 月相
//  在 UTC 0h..24h 内以 30 分钟步长搜索高度角过零点
// ================================================================

FnTwilight::MoonInfo FnTwilight::_calcMoonInfo(double jd, double latDeg, double lonDeg) {
  MoonInfo mi;
  memset(&mi, 0, sizeof(mi));
  mi.noRise = mi.noSet = true;
  mi.moonriseMin = mi.moonsetMin = -1;

  // 高度角过零点搜索（步长 30 min，48 步 = 24 h）
  double prevAlt = _moonAltitude(jd, latDeg, lonDeg);
  for (int i = 1; i <= 48; i++) {
    double t      = i * (30.0 / 1440.0);
    double curAlt = _moonAltitude(jd + t, latDeg, lonDeg);

    if (prevAlt < 0.0 && curAlt >= 0.0) {
      // 月出：插值
      double frac = (-prevAlt) / (curAlt - prevAlt);
      int m = (int)round(((i - 1) + frac) * 30.0);
      if (mi.noRise) { mi.moonriseMin = m; mi.noRise = false; }
    } else if (prevAlt >= 0.0 && curAlt < 0.0) {
      // 月落：插值
      double frac = prevAlt / (prevAlt - curAlt);
      int m = (int)round(((i - 1) + frac) * 30.0);
      if (mi.noSet) { mi.moonsetMin = m; mi.noSet = false; }
    }
    prevAlt = curAlt;
  }

  // 月龄（参考朔：JD 2451549.77 = 2000-01-06 18:14 UTC）
  static const double REF_NEW_MOON = 2451549.77;
  static const double SYNODIC      = 29.530588853;
  double age = fmod(jd - REF_NEW_MOON, SYNODIC);
  if (age < 0.0) age += SYNODIC;
  mi.agedays      = (float)age;
  mi.illumination = (float)((1.0 - cos(2.0 * M_PI * age / SYNODIC)) / 2.0);

  return mi;
}

// ================================================================
//  月相名称
// ================================================================

const char* FnTwilight::_moonPhaseName(float age) {
  if (age <  1.85f) return "New Moon";
  if (age <  7.38f) return "Waxing Crescent";
  if (age <  9.22f) return "First Quarter";
  if (age < 14.77f) return "Waxing Gibbous";
  if (age < 16.61f) return "Full Moon";
  if (age < 22.15f) return "Waning Gibbous";
  if (age < 23.99f) return "Last Quarter";
  return "Waning Crescent";
}

// ================================================================
//  生命周期
// ================================================================

void FnTwilight::onEnter() {
  _page  = 0;
  _dirty = true;
}

bool FnTwilight::needsRedraw(unsigned long /*now*/) {
  return _dirty;
}

void FnTwilight::onUpdate(bool /*force*/) {
  _dirty = false;
  GPSManager& gps = GPSManager::instance();
  RTCManager& rtc = RTCManager::instance();

  bool hasPos  = gps.hasFreshFix();
  // 优先使用 RTC 本地日期；若 RTC 未同步则直接用 GPS UTC 日期
  bool hasDate = rtc.dateValid() || gps.dateValid();
  int  tzOff   = rtc.timezoneOffset();

  int yr = rtc.dateValid() ? rtc.localYear()  : gps.utcYear();
  int mo = rtc.dateValid() ? rtc.localMonth() : gps.utcMonth();
  int dy = rtc.dateValid() ? rtc.localDay()   : gps.utcDay();

  if (_page == 0) {
    if (hasPos && hasDate) {
      double jd = _calcJD(yr, mo, dy);
      SunTimes st = _calcAllSunTimes(jd, gps.latitude(), gps.longitude());
      _drawSolarPage(st, true, gps.latitude(), gps.longitude(), tzOff);
    } else {
      SunTimes st; memset(&st, 0, sizeof(st)); st.alwaysDown = true;
      _drawSolarPage(st, false, 0.0, 0.0, tzOff);
    }
  } else {
    if (hasPos && hasDate) {
      double jd = _calcJD(yr, mo, dy);
      MoonInfo mi = _calcMoonInfo(jd, gps.latitude(), gps.longitude());
      _drawLunarPage(mi, true, tzOff);
    } else {
      MoonInfo mi; memset(&mi, 0, sizeof(mi)); mi.noRise = mi.noSet = true;
      _drawLunarPage(mi, false, tzOff);
    }
  }
}

bool FnTwilight::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;
  if (event.key == 0x09 || event.key == '.' || event.key == '/'
      || event.key == ';' || event.key == ',') {
    _page  = (_page + 1) % 2;
    _dirty = true;
    return true;
  }
  return false;
}

// ================================================================
//  绘图 — 太阳页
//  风格参照 fn_peers / fn_gps_dashboard._drawTabFix()
// ================================================================

void FnTwilight::_drawSolarPage(const SunTimes& st, bool hasPos,
                                 double lat, double lon, int tzOff) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  char buf[32];

  // ── 标题行（与 fn_gps_dashboard / fn_trip 一致）─────
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  cv.print("Solar");
  cv.setCursor(SCREEN_W - 36, 2);
  cv.print("[Tab]");

  // ── 日出 / 日落 标签 ────────────────────────────
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 13);
  cv.print("Sunrise");
  cv.setCursor(128, 13);
  cv.print("Sunset");

  // ── 日出 / 日落 时间（size=2）───────────────────
  cv.setTextSize(2);
  if (!hasPos || (st.alwaysDown && !st.alwaysUp)) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4,   22); cv.print("--:--");
    cv.setCursor(128, 22); cv.print("--:--");
  } else if (st.alwaysUp) {
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(4,   22); cv.print("Polar");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(128, 22); cv.print("Day");
  } else {
    cv.setTextColor(TFT_GREEN);
    _fmtLocal(st.sunriseMin, tzOff, buf, sizeof(buf));
    cv.setCursor(4, 22); cv.print(buf);
    cv.setTextColor(TFT_YELLOW);
    _fmtLocal(st.sunsetMin, tzOff, buf, sizeof(buf));
    cv.setCursor(128, 22); cv.print(buf);
  }

  // ── 昼长 / 夜长 ────────────────────────────────
  cv.setTextSize(1);
  if (hasPos && !st.alwaysDown && !st.alwaysUp
      && st.sunriseMin != -1 && st.sunsetMin != -1) {
    int dayMin   = st.sunsetMin - st.sunriseMin;
    if (dayMin < 0) dayMin = 0;
    int nightMin = 1440 - dayMin;
    cv.setTextColor(TFT_LIGHTGREY);
    cv.setCursor(4, 40);
    _fmtDuration(dayMin, buf, sizeof(buf));
    cv.printf("Day %s", buf);
    cv.setCursor(128, 40);
    _fmtDuration(nightMin, buf, sizeof(buf));
    cv.printf("Night %s", buf);
  } else if (hasPos && st.alwaysUp) {
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(4, 40); cv.print("Day 24h00m");
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(128, 40); cv.print("Night 0h");
  } else if (hasPos && st.alwaysDown) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, 40); cv.print("Day 0h");
    cv.setTextColor(TFT_LIGHTGREY);
    cv.setCursor(128, 40); cv.print("Night 24h00m");
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, 40); cv.print("No GPS fix");
  }

  // ── 蒙影时间（label x=4, dawn x=68, ~ x=104, dusk x=116）
  auto drawTwilightRow = [&](int y, const char* label,
                              int dawnMin, int duskMin) {
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, y);
    cv.print(label);
    if (hasPos && dawnMin != -1 && duskMin != -1) {
      cv.setTextColor(TFT_WHITE);
      _fmtLocal(dawnMin, tzOff, buf, sizeof(buf));
      cv.setCursor(68, y); cv.print(buf);
      cv.setTextColor(TFT_DARKGREY);
      cv.setCursor(104, y); cv.print("~");
      cv.setTextColor(TFT_WHITE);
      _fmtLocal(duskMin, tzOff, buf, sizeof(buf));
      cv.setCursor(116, y); cv.print(buf);
    } else {
      cv.setTextColor(TFT_DARKGREY);
      cv.setCursor(68, y); cv.print("--:--  ~  --:--");
    }
  };

  drawTwilightRow(55, "Civil",    st.civilDawnMin, st.civilDuskMin);
  drawTwilightRow(67, "Nautical", st.nautDawnMin,  st.nautDuskMin);
  drawTwilightRow(79, "Astron.",  st.astroDawnMin, st.astroDuskMin);

  // ── 正午 ────────────────────────────────────────
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 91);
  cv.print("Solar Noon");
  cv.setTextColor(hasPos && st.solarNoonMin > 0 ? TFT_WHITE : TFT_DARKGREY);
  if (hasPos && st.solarNoonMin > 0) {
    _fmtLocal(st.solarNoonMin, tzOff, buf, sizeof(buf));
    cv.setCursor(68, 91); cv.print(buf);
  } else {
    cv.setCursor(68, 91); cv.print("--:--");
  }

  // ── 坐标 ────────────────────────────────────────
  if (hasPos) {
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, 103);
    snprintf(buf, sizeof(buf), "%.2f%c %.2f%c",
             fabs(lat), lat >= 0 ? 'N' : 'S',
             fabs(lon), lon >= 0 ? 'E' : 'W');
    cv.print(buf);
  }

}

// ================================================================
//  绘图 — 月亮页
// ================================================================

void FnTwilight::_drawLunarPage(const MoonInfo& mi, bool hasPos, int tzOff) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  char buf[32];

  // ── 标题行 ──────────────────────────────────────
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  cv.print("Lunar");
  cv.setCursor(SCREEN_W - 36, 2);
  cv.print("[Tab]");

  // ── 月出 / 月落 标签 ────────────────────────────
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 13);
  cv.print("Moonrise");
  cv.setCursor(128, 13);
  cv.print("Moonset");

  // ── 月出 / 月落 时间（size=2）───────────────────
  cv.setTextSize(2);
  if (!hasPos || mi.noRise) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, 22); cv.print("--:--");
  } else {
    cv.setTextColor(TFT_CYAN);
    _fmtLocal(mi.moonriseMin, tzOff, buf, sizeof(buf));
    cv.setCursor(4, 22); cv.print(buf);
  }
  if (!hasPos || mi.noSet) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(128, 22); cv.print("--:--");
  } else {
    cv.setTextColor(TFT_LIGHTGREY);
    _fmtLocal(mi.moonsetMin, tzOff, buf, sizeof(buf));
    cv.setCursor(128, 22); cv.print(buf);
  }

  // ── 月相信息（label x=4, value x=68，与 fix-info 一致）
  const int LX = 4, VX = 68;
  int y = 46;
  const int lh = 12;
  cv.setTextSize(1);

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(LX, y); cv.print("Phase:");
  cv.setTextColor(hasPos ? TFT_WHITE : TFT_DARKGREY);
  cv.setCursor(VX, y);
  cv.print(hasPos ? _moonPhaseName(mi.agedays) : "--");
  y += lh;

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(LX, y); cv.print("Illumin:");
  cv.setCursor(VX, y);
  if (hasPos) {
    cv.setTextColor(TFT_WHITE);
    snprintf(buf, sizeof(buf), "%.0f%%", mi.illumination * 100.0f);
    cv.print(buf);
    // 小型照度进度条
    int barX = VX + 30, barY = y + 1, barW = 100, barH = 6;
    cv.drawRect(barX, barY, barW + 2, barH + 2, TFT_DARKGREY);
    int fill = (int)(mi.illumination * barW);
    if (fill > 0) cv.fillRect(barX + 1, barY + 1, fill, barH, TFT_CYAN);
  } else {
    cv.setTextColor(TFT_DARKGREY); cv.print("--");
  }
  y += lh;

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(LX, y); cv.print("Age:");
  cv.setTextColor(hasPos ? TFT_WHITE : TFT_DARKGREY);
  cv.setCursor(VX, y);
  if (hasPos) { snprintf(buf, sizeof(buf), "%.1f days", mi.agedays); cv.print(buf); }
  else { cv.print("--"); }
  y += lh;

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(LX, y); cv.print("Next:");
  if (hasPos) {
    float age = mi.agedays;
    static const float phases[] = { 0.0f, 7.38f, 14.77f, 22.15f, 29.53f };
    static const char* phNames[] = { "New Moon", "1st Qtr", "Full Moon", "Last Qtr", "New Moon" };
    const char* nextName = phNames[4]; float nextAt = 29.53f;
    for (int i = 0; i < 5; i++) {
      if (phases[i] > age + 0.5f) { nextName = phNames[i]; nextAt = phases[i]; break; }
    }
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(VX, y);
    snprintf(buf, sizeof(buf), "%s  %.1fd", nextName, nextAt - age);
    cv.print(buf);
  } else {
    cv.setTextColor(TFT_DARKGREY); cv.setCursor(VX, y); cv.print("--");
  }

}

// ================================================================
//  图标：日出样式（水平线 + 上升半圆 + 放射线）
// ================================================================

void FnTwilight::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();

  int cx = x + size / 2;
  int cy = y + size / 2;
  int s  = size;

  // ── 地平线 ──────────────────────────────────────
  int hY  = cy + s / 8;           // 地平线稍低于中心
  int hX1 = x + s * 10 / 100;
  int hX2 = x + s * 90 / 100;
  int lineW = (s > 30) ? 3 : 2;
  for (int i = 0; i < lineW; i++) {
    cv.drawLine(hX1, hY + i, hX2, hY + i, color);
  }

  // ── 半圆太阳（地平线以上）─────────────────────
  int r = s * 24 / 100;
  // 用 fillArc 不存在，改用多个 fillCircle 裁剪
  cv.fillCircle(cx, hY, r, color);
  // 遮掉地平线以下部分
  cv.fillRect(x, hY + lineW, s, r + 4, 0x0000);  // 用黑色覆盖下半
  // 中心孔（使其像环形）——可选，若图标够大再加
  if (r > 10) {
    cv.fillCircle(cx, hY, r * 55 / 100, (uint16_t)0x0000);
  }

  // ── 放射线（地平线以上 3 条）──────────────────
  int rayLen = s * 14 / 100;
  if (rayLen < 3) rayLen = 3;
  // 正上方
  cv.drawLine(cx, hY - r - 2, cx, hY - r - 2 - rayLen, color);
  if (s > 20) {
    // 左上 45°
    int rx = (int)((r + 2) * 0.707f), ry = (int)((r + 2) * 0.707f);
    cv.drawLine(cx - rx, hY - ry, cx - rx - (int)(rayLen*0.707f), hY - ry - (int)(rayLen*0.707f), color);
    // 右上 45°
    cv.drawLine(cx + rx, hY - ry, cx + rx + (int)(rayLen*0.707f), hY - ry - (int)(rayLen*0.707f), color);
  }
}
