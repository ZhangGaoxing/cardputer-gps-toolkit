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

class GPSManager {
public:
  static GPSManager& instance();

  /** 启动GPS串口并发送初始化命令 */
  void begin();

  /** 每帧调用：读取串口，解析NMEA，更新卫星数据 */
  void update();

  /** 是否获取到有效定位 */
  bool hasFix() { return _gps.location.isValid(); }

  // 标准GPS数据存取器（非const，因为TinyGPSPlus内部会更新状态）
  float latitude()          { return (float)_gps.location.lat(); }
  float longitude()         { return (float)_gps.location.lng(); }
  float altitude()          { return _gps.altitude.isValid() ? (float)_gps.altitude.meters() : 0.0f; }
  float speedKmph()         { return (float)_gps.speed.kmph(); }
  float courseDeg()         { return (float)_gps.course.deg(); }
  int   satellitesUsed()    { return _gps.satellites.isValid() ? (int)_gps.satellites.value() : 0; }

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
  int ggaFixQuality() const { return _ggaFixQuality; }
  int gsaFixMode() const    { return _gsaFixMode; }
  float pdop() const        { return _pdop; }
  float vdop() const        { return _vdop; }
  float hdop()              { return (float)_gps.hdop.hdop(); }
  float geoidHeight() const { return _geoidHeight; }
  bool geoidValid() const   { return _geoidValid; }

  // 卫星数据
  const std::vector<SatData>& satellites() const { return _satellites; }

  // NMEA监视器数据
  const char* nmeaLine(int index) const;
  int nmeaLineCount() const { return _nmeaBufCount; }

  // GPS串口状态
  GPSState state() const { return _gpsState; }

private:
  GPSManager() = default;
  GPSManager(const GPSManager&) = delete;
  GPSManager& operator=(const GPSManager&) = delete;

  // 串口操作
  void _sendPMTK(const char* body);
  void _openSerial();
  void _closeSerial();

  // NMEA解析
  void _nmeaDispatcher(const String& line);
  void _parseGSV(const String& line);
  void _parseGSA(const String& line);
  void _parseGGA(const String& line);
  GSVSequenceState* _getGSVState(const String& system);
  void _storeSatellite(const SatData& sat);

  HardwareSerial _serial{2};
  TinyGPSPlus _gps;
  GPSState _gpsState = GPS_ON;
  unsigned long _lastValidGpsMillis = 0;

  // 卫星数据
  std::vector<SatData> _satellites;
  GSVSequenceState _gsvStates[5];
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
};

#endif // GPS_MANAGER_H
