/**
 * gps_manager.h — GPS管理器
 * 管理GPS串口通信、NMEA解析、卫星跟踪
 */
#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <M5Cardputer.h>
#include <TinyGPSPlus.h>
#include <vector>
#include "config.h"

// 卫星数据结构
struct SatData {
  String system;   // "GPS", "GLONASS", "Galileo", "BeiDou", "QZSS"
  int id;
  int elevation;   // 0-90°
  int azimuth;     // 0-359°
  int snr;         // 0-99
  bool used;       // 用于定位解算
  bool visible;    // 最近周期可见
};

// GSV多消息序列状态
struct GSVSequenceState {
  String system;
  int totalMsgs = 0;
  int lastMsgNum = 0;
  std::vector<int> currentVisible;
};

struct ReliableFixSnapshot {
  bool valid = false;
  float lat = 0.0f;
  float lon = 0.0f;
  float altM = 0.0f;
  bool altValid = false;
  float hdop = 99.9f;
  int satellitesUsed = 0;
  int fixQuality = 0;
  int fixMode = 1;
  bool timeValid = false;
  bool dateValid = false;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  uint32_t capturedAtMs = 0;
};

class GPSManager {
public:
  static GPSManager& instance();

  /** 启动GPS串口并发送初始化命令 */
  void begin();

  /** 每帧调用：读取串口，解析NMEA，更新卫星数据 */
  void update();

  /** 是否有当前新鲜定位（兼容旧调用语义） */
  bool hasFix() const;
  bool hasFreshFix() const;
  bool hasReliableFix() const;
  uint32_t fixAgeMs() const;
  bool hasRecentGnssData() const;
  bool hasLastReliableFix() const { return _lastReliableFix.valid; }
  uint32_t lastReliableFixAgeMs() const;
  const ReliableFixSnapshot& lastReliableFix() const { return _lastReliableFix; }

  // 标准GPS数据存取器（非const，因为TinyGPSPlus内部会更新状态）
  float latitude()          { return (float)_gps.location.lat(); }
  float longitude()         { return (float)_gps.location.lng(); }
  float altitude()          { return _gps.altitude.isValid() ? (float)_gps.altitude.meters() : 0.0f; }
  bool altitudeValid()      { return _gps.altitude.isValid() && _gps.altitude.age() <= GPS_FIX_MAX_AGE_MS; }
  float speedKmph()         { return (float)_gps.speed.kmph(); }
  float speedMps()          { return (float)_gps.speed.mps(); }
  float courseDeg()         { return (float)_gps.course.deg(); }
  bool speedValid()         { return _gps.speed.isValid() && _gps.speed.age() <= GPS_FIX_MAX_AGE_MS; }
  bool courseValid()        { return _gps.course.isValid() && _gps.course.age() <= GPS_FIX_MAX_AGE_MS; }
  int   satellitesUsed() const;

  // UTC时间和日期
  int utcHour()             { return (int)_gps.time.hour(); }
  int utcMinute()           { return (int)_gps.time.minute(); }
  int utcSecond()           { return (int)_gps.time.second(); }
  int utcYear()             { return (int)_gps.date.year(); }
  int utcMonth()            { return (int)_gps.date.month(); }
  int utcDay()              { return (int)_gps.date.day(); }
  bool timeValid()          { return _gps.time.isValid(); }
  bool dateValid()          { return _gps.date.isValid(); }

  // 定位质量数据
  int fixQuality() const    {
    return (_lastGgaMillis != 0 && millis() - _lastGgaMillis <= GPS_FIX_MAX_AGE_MS)
           ? _ggaFixQuality : 0;
  }
  int fixMode() const       {
    return (_lastGsaMillis != 0 && millis() - _lastGsaMillis <= GPS_FIX_MAX_AGE_MS)
           ? _gsaFixMode : 1;
  }
  int ggaFixQuality() const { return fixQuality(); }
  int gsaFixMode() const    { return fixMode(); }
  float pdop() const;
  float vdop() const;
  float hdop() const;
  float geoidHeight() const { return _geoidHeight; }
  bool geoidValid() const   { return _geoidValid; }

  // 卫星数据
  const std::vector<SatData>& satellites() const { return _satellites; }

  // NMEA监视器数据
  const char* nmeaLine(int index) const;
  int nmeaLineCount() const { return _nmeaBufCount; }

  // GPS串口状态
  GPSState state() const { return _gpsState; }

  /** 启用/禁用 GPS 省电模式（ATGM336H AlwaysLocate™ 定期模式） */
  void enablePowerSave(bool enable);
  bool isPowerSaveActive() const { return _powerSaveActive; }

private:
  GPSManager() = default;
  GPSManager(const GPSManager&) = delete;
  GPSManager& operator=(const GPSManager&) = delete;

  // 串口操作
  void _sendPMTK(const char* body);
  void _openSerial();
  void _closeSerial();

  // NMEA解析（使用 const char* 避免堆碎片化）
  void _nmeaDispatcher(const char* line);
  void _parseGSV(const char* line);
  void _parseGSA(const char* line);
  void _parseGGA(const char* line);
  void _clearUsedFlags();
  void _markUsedSatellite(const char* preferredSystem, int id);
  GSVSequenceState* _getGSVState(const char* system);
  void purgeStaleSatellites();
  void _storeSatellite(const SatData& sat);
  void _captureReliableFixSnapshot();
  bool _hasRecentParsedNmea() const;
  void _updateGpsState();

  HardwareSerial _serial{2};
  TinyGPSPlus _gps;
  GPSState _gpsState = GPS_ON;
  unsigned long _lastValidGpsMillis = 0;
  unsigned long _lastNmeaMillis = 0;
  unsigned long _lastGgaMillis = 0;
  unsigned long _satPurgeCheckMs = 0;
  bool _powerSaveActive = false;

  // 卫星数据
  std::vector<SatData> _satellites;
  GSVSequenceState _gsvStates[6];
  int _gsvCount = 0;

  // 定位质量
  int _ggaFixQuality = 0;
  int _gsaFixMode = 1;
  float _pdop = 99.9f;
  float _vdop = 99.9f;
  float _geoidHeight = 0;
  bool _geoidValid = false;

  // NMEA缓冲
  char _nmeaBuf[NMEA_BUF_LINES][NMEA_BUF_WIDTH] = {};
  int _nmeaBufHead = 0;
  int _nmeaBufCount = 0;

  // 调试标志
  bool _nmeaSerial = false;
  bool _satListSerial = false;
  unsigned long _lastGsaMillis = 0;
  ReliableFixSnapshot _lastReliableFix;
};

#endif // GPS_MANAGER_H
