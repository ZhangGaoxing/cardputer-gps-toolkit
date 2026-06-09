/**
 * rtc_manager.cpp — RTC时间管理器实现
 * DST计算支持：北美(US/Canada)、欧洲(EU)、南澳大利亚、新西兰
 */
#include "rtc_manager.h"
#include <time.h>
#include <math.h>

RTCManager& RTCManager::instance() {
  static RTCManager rtc;
  return rtc;
}

void RTCManager::begin() {
  _synced = false;
}

void RTCManager::update(int utcYear, int utcMonth, int utcDay,
                         int utcHour, int utcMinute, int utcSecond,
                         float latitude, float longitude,
                         bool locValid, bool dateValid) {
  if (!dateValid) {
    // 没有日期信息，只能做简单的小时偏移
    if (locValid) {
      _stdOffset = (int)roundf(longitude / 15.0f);
      _localHour = (utcHour + _stdOffset + 24) % 24;
      _localMinute = utcMinute;
      _localSecond = utcSecond;
    }
    return;
  }

  // 计算标准时区偏移
  if (locValid) {
    _stdOffset = (int)roundf(longitude / 15.0f);
    _dstAdjust = _calcDST(utcYear, utcMonth, utcDay, utcHour, latitude, longitude);
  }

  // 通过mktime/gmtime进行UTC→本地时间转换
  struct tm utcTm = {};
  utcTm.tm_year = utcYear - 1900;
  utcTm.tm_mon  = utcMonth - 1;
  utcTm.tm_mday = utcDay;
  utcTm.tm_hour = utcHour;
  utcTm.tm_min  = utcMinute;
  utcTm.tm_sec  = utcSecond;

  time_t utcEpoch = mktime(&utcTm);
  _syncEpoch = utcEpoch;
  _syncMillis = millis();
  _synced = true;
  _dateValid = true;

  // 计算本地时间
  time_t localEpoch = utcEpoch + (long)(_stdOffset + _dstAdjust) * 3600L;
  struct tm localTm;
  gmtime_r(&localEpoch, &localTm);

  _localYear   = localTm.tm_year + 1900;
  _localMonth  = localTm.tm_mon + 1;
  _localDay    = localTm.tm_mday;
  _localHour   = localTm.tm_hour;
  _localMinute = localTm.tm_min;
  _localSecond = localTm.tm_sec;
}

int RTCManager::localHour() const {
  if (!_synced) return 0;
  unsigned long elapsed = (millis() - _syncMillis) / 1000UL;
  time_t nowEpoch = _syncEpoch + elapsed + (long)(_stdOffset + _dstAdjust) * 3600L;
  struct tm nowTm;
  gmtime_r(&nowEpoch, &nowTm);
  return nowTm.tm_hour;
}

int RTCManager::localMinute() const {
  if (!_synced) return 0;
  unsigned long elapsed = (millis() - _syncMillis) / 1000UL;
  time_t nowEpoch = _syncEpoch + elapsed + (long)(_stdOffset + _dstAdjust) * 3600L;
  struct tm nowTm;
  gmtime_r(&nowEpoch, &nowTm);
  return nowTm.tm_min;
}

int RTCManager::localSecond() const {
  if (!_synced) return 0;
  unsigned long elapsed = (millis() - _syncMillis) / 1000UL;
  time_t nowEpoch = _syncEpoch + elapsed + (long)(_stdOffset + _dstAdjust) * 3600L;
  struct tm nowTm;
  gmtime_r(&nowEpoch, &nowTm);
  return nowTm.tm_sec;
}

// ==================================================================
//  DST计算辅助函数
// ==================================================================

int RTCManager::_calcDow(int y, int m, int d) {
  static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) y--;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

int RTCManager::_nthWday(int y, int m, int wday, int n) {
  int d1 = _calcDow(y, m, 1);
  return 1 + ((wday - d1 + 7) % 7) + (n - 1) * 7;
}

int RTCManager::_lastWday(int y, int m, int wday) {
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int days = dim[m - 1];
  if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) days = 29;
  int dl = _calcDow(y, m, days);
  return days - ((dl - wday + 7) % 7);
}

int RTCManager::_calcDST(int y, int mon, int d, int utcH, float lat, float lon) {
  // 北美(US/Canada): 3月第二个周日 → 11月第一个周日
  if (lat > 24 && lat < 72 && lon > -140 && lon < -50) {
    if (lat < 23 && lon < -154) return 0;  // 夏威夷无DST
    if (lat > 31 && lat < 37.5f && lon > -115 && lon < -109) return 0;  // 亚利桑那无DST
    int sD = _nthWday(y, 3, 0, 2);
    int eD = _nthWday(y, 11, 0, 1);
    int so = (int)roundf(lon / 15.0f);
    int lh = utcH + so, ld = d;
    if (lh >= 24) { lh -= 24; ld++; } else if (lh < 0) { lh += 24; ld--; }
    if (mon > 3 && mon < 11) return 1;
    if (mon < 3 || mon > 11) return 0;
    if (mon == 3)  return (ld > sD || (ld == sD && lh >= 2)) ? 1 : 0;
    if (mon == 11) return (ld < eD || (ld == eD && lh < 1)) ? 1 : 0;
    return 0;
  }

  // 欧洲(EU): 3月最后一个周日 → 10月最后一个周日 (1:00 UTC)
  if (lat > 34 && lat < 72 && lon > -12 && lon < 45) {
    int sD = _lastWday(y, 3, 0), eD = _lastWday(y, 10, 0);
    if (mon > 3 && mon < 10) return 1;
    if (mon < 3 || mon > 10) return 0;
    if (mon == 3)  return (d > sD || (d == sD && utcH >= 1)) ? 1 : 0;
    if (mon == 10) return (d < eD || (d == eD && utcH < 1)) ? 1 : 0;
    return 0;
  }

  // 南澳大利亚: 10月第一个周日 → 4月第一个周日
  if (lat < -28 && lon > 138 && lon < 155) {
    int sD = _nthWday(y, 10, 0, 1), eD = _nthWday(y, 4, 0, 1);
    int lh = utcH + 10, ld = d;
    if (lh >= 24) { lh -= 24; ld++; }
    if (mon > 10 || mon < 4) return 1;
    if (mon > 4 && mon < 10) return 0;
    if (mon == 10) return (ld > sD || (ld == sD && lh >= 2)) ? 1 : 0;
    if (mon == 4)  return (ld < eD || (ld == eD && lh < 2)) ? 1 : 0;
    return 0;
  }

  // 新西兰: 9月最后一个周日 → 4月第一个周日
  if (lat < -34 && lon > 165) {
    int sD = _lastWday(y, 9, 0), eD = _nthWday(y, 4, 0, 1);
    int lh = utcH + 12, ld = d;
    if (lh >= 24) { lh -= 24; ld++; }
    if (mon > 9 || mon < 4) return 1;
    if (mon > 4 && mon < 9) return 0;
    if (mon == 9) return (ld > sD || (ld == sD && lh >= 2)) ? 1 : 0;
    if (mon == 4) return (ld < eD || (ld == eD && lh < 2)) ? 1 : 0;
    return 0;
  }

  return 0;  // 其他地区无DST
}
