/**
 * rtc_manager.h — RTC时间管理器
 * 利用GPS定位信息计算时区偏移和DST，通过millis()推算当前本地时间
 */
#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

class RTCManager {
public:
  /** 获取单例实例 */
  static RTCManager& instance();

  /** 初始化 */
  void begin();

  /** 检查GPS数据并尝试同步时间 */
  void update(int utcYear, int utcMonth, int utcDay,
              int utcHour, int utcMinute, int utcSecond,
              float latitude, float longitude, bool locValid, bool dateValid);

  /** 是否已完成GPS时间同步 */
  bool isSynced() const { return _synced; }

  /** 获取当前本地时间 */
  int localHour() const;
  int localMinute() const;
  int localSecond() const;
  int localYear() const   { return _localYear; }
  int localMonth() const  { return _localMonth; }
  int localDay() const    { return _localDay; }

  /** 日期是否有效 */
  bool dateValid() const { return _dateValid; }

  /** 获取时区偏移（标准偏移 + DST调整） */
  int timezoneOffset() const { return _stdOffset + _dstAdjust; }

  /** 距离上次GPS同步的秒数 */
  unsigned long syncAge() const {
    if (!_synced) return 99999;
    return (millis() - _syncMillis) / 1000UL;
  }

private:
  RTCManager() = default;
  RTCManager(const RTCManager&) = delete;
  RTCManager& operator=(const RTCManager&) = delete;

  /** 计算星期几 (0=Sun..6=Sat) Tomohiko Sakamoto算法 */
  static int _calcDow(int y, int m, int d);

  /** 计算某月第N个指定星期几的日期 */
  static int _nthWday(int y, int m, int wday, int n);

  /** 计算某月最后一个指定星期几的日期 */
  static int _lastWday(int y, int m, int wday);

  /** 根据位置和UTC时间计算DST调整(0或1小时) */
  static int _calcDST(int year, int month, int day, int utcHour,
                       float lat, float lon);

  // 同步状态
  bool _synced = false;
  bool _dateValid = false;
  unsigned long _syncEpoch = 0;   // GPS UTC时间的epoch秒
  unsigned long _syncMillis = 0;  // 同步时的millis()
  int _stdOffset = 8;            // 标准时区偏移（经度/15取整）
  int _dstAdjust = 0;            // 夏令时调整（0或1）

  // 缓存的本地时间
  int _localYear = 2026, _localMonth = 6, _localDay = 8;
  int _localHour = 0, _localMinute = 0, _localSecond = 0;
};

#endif // RTC_MANAGER_H
