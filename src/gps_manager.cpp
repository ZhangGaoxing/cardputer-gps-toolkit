/**
 * gps_manager.cpp — GPS管理器实现
 * 【占位实现 - Phase 2将完整移植GPSInfo的NMEA解析代码】
 */
#include "gps_manager.h"
#include <algorithm>

GPSManager& GPSManager::instance() {
  static GPSManager gm;
  return gm;
}

void GPSManager::begin() {
  _lastReliableFix = ReliableFixSnapshot();
  _openSerial();
}

// ==================================================================
//  串口操作
// ==================================================================

void GPSManager::_sendPMTK(const char* body) {
  uint8_t ck = 0;
  for (const char* p = body; *p; p++) ck ^= *p;
  char cmd[80];
  snprintf(cmd, sizeof(cmd), "$%s*%02X\r\n", body, ck);
  _serial.print(cmd);
}

void GPSManager::_openSerial() {
  _serial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(200);
  _sendPMTK("PCAS03,1,1,1,1,1,1,0,0");
  _sendPMTK("PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0");
  _lastValidGpsMillis = millis();
  _lastNmeaMillis = 0;
  _lastGgaMillis = 0;
  _lastGsaMillis = 0;
}

void GPSManager::_closeSerial() {
  _serial.end();
}

// ==================================================================
//  主更新循环
// ==================================================================

void GPSManager::update() {
  // 固定尺寸行缓冲，消除逐字符 String += 导致的堆碎片化
  static char nmeaLineBuf[NMEA_BUF_WIDTH] = {};
  static int  nmeaLineLen = 0;
  bool gotValidChar = false;

  // 超时检测
  if (_serial.available() == 0 &&
      millis() - _lastValidGpsMillis > GPS_TIMEOUT) {
    _gpsState = GPS_ERR;
  }

  while (_serial.available()) {
    char c = _serial.read();
    _gps.encode(c);

    if (c != '\r' && c != '\n') gotValidChar = true;

    if (c == '\n') {
      nmeaLineBuf[nmeaLineLen] = '\0';
      _nmeaDispatcher(nmeaLineBuf);
      nmeaLineLen = 0;
    } else if (c != '\r') {
      if (nmeaLineLen < NMEA_BUF_WIDTH - 1) nmeaLineBuf[nmeaLineLen++] = c;
    }
  }

  if (gotValidChar) {
    _lastValidGpsMillis = millis();
  }

  _updateGpsState();
  if (_gpsState == GPS_RELIABLE_FIX) {
    _captureReliableFixSnapshot();
  }

  // 信号长时间丢失后清除陈旧卫星列表（>60s 无 GNSS 数据）
  if (!hasRecentGnssData()) {
    if (_satPurgeCheckMs == 0) {
      _satPurgeCheckMs = millis();
    } else if (millis() - _satPurgeCheckMs >= 60000UL) {
      purgeStaleSatellites();
      _satPurgeCheckMs = 0;
    }
  } else {
    _satPurgeCheckMs = 0;
  }
}

bool GPSManager::hasFix() const {
  return hasFreshFix();
}

bool GPSManager::hasFreshFix() const {
  return _gps.location.isValid() &&
         fixAgeMs() <= GPS_FIX_MAX_AGE_MS &&
         hasRecentGnssData() &&
         _hasRecentParsedNmea();
}

bool GPSManager::hasReliableFix() const {
  return hasFreshFix() &&
         fixQuality() > 0 &&
         (fixMode() == 2 || fixMode() == 3) &&
         satellitesUsed() >= GPS_MIN_SATELLITES_USED &&
         _gps.hdop.isValid() &&
         _gps.hdop.age() <= GPS_FIX_MAX_AGE_MS &&
         hdop() < GPS_RELIABLE_HDOP_MAX &&
         _lastGgaMillis != 0 &&
         millis() - _lastGgaMillis <= GPS_FIX_MAX_AGE_MS &&
         _lastGsaMillis != 0 &&
         millis() - _lastGsaMillis <= GPS_FIX_MAX_AGE_MS;
}

uint32_t GPSManager::fixAgeMs() const {
  if (!_gps.location.isValid()) return UINT32_MAX;
  return _gps.location.age();
}

uint32_t GPSManager::lastReliableFixAgeMs() const {
  if (!_lastReliableFix.valid) return UINT32_MAX;
  return millis() - _lastReliableFix.capturedAtMs;
}

bool GPSManager::hasRecentGnssData() const {
  return _lastValidGpsMillis != 0 &&
         millis() - _lastValidGpsMillis <= GPS_NMEA_MAX_AGE_MS;
}

bool GPSManager::_hasRecentParsedNmea() const {
  return _lastNmeaMillis != 0 &&
         millis() - _lastNmeaMillis <= GPS_NMEA_MAX_AGE_MS;
}

int GPSManager::satellitesUsed() const {
  if (!_gps.satellites.isValid() ||
      _gps.satellites.age() > GPS_FIX_MAX_AGE_MS) {
    return 0;
  }
  return (int)const_cast<TinyGPSInteger&>(_gps.satellites).value();
}

float GPSManager::hdop() const {
  if (!_gps.hdop.isValid() ||
      _gps.hdop.age() > GPS_FIX_MAX_AGE_MS) {
    return 99.9f;
  }
  return (float)const_cast<TinyGPSHDOP&>(_gps.hdop).hdop();
}

float GPSManager::pdop() const {
  if (_lastGsaMillis == 0 || millis() - _lastGsaMillis > GPS_FIX_MAX_AGE_MS) {
    return 99.9f;
  }
  return _pdop;
}

float GPSManager::vdop() const {
  if (_lastGsaMillis == 0 || millis() - _lastGsaMillis > GPS_FIX_MAX_AGE_MS) {
    return 99.9f;
  }
  return _vdop;
}

void GPSManager::_captureReliableFixSnapshot() {
  _lastReliableFix.valid = true;
  _lastReliableFix.lat = latitude();
  _lastReliableFix.lon = longitude();
  _lastReliableFix.altM = altitude();
  _lastReliableFix.altValid = altitudeValid();
  _lastReliableFix.hdop = hdop();
  _lastReliableFix.satellitesUsed = satellitesUsed();
  _lastReliableFix.fixQuality = fixQuality();
  _lastReliableFix.fixMode = fixMode();
  _lastReliableFix.timeValid = timeValid();
  _lastReliableFix.dateValid = dateValid();
  _lastReliableFix.year = utcYear();
  _lastReliableFix.month = utcMonth();
  _lastReliableFix.day = utcDay();
  _lastReliableFix.hour = utcHour();
  _lastReliableFix.minute = utcMinute();
  _lastReliableFix.second = utcSecond();
  _lastReliableFix.capturedAtMs = millis();
}

void GPSManager::_updateGpsState() {
  if (!hasRecentGnssData()) {
    _gpsState = GPS_ERR;
  } else if (hasReliableFix()) {
    _gpsState = GPS_RELIABLE_FIX;
  } else if (hasFreshFix()) {
    _gpsState = GPS_FIX;
  } else {
    _gpsState = GPS_SEARCHING;
  }
}

// ==================================================================
//  NMEA 分发和解析
// ==================================================================

// NMEA 校验和验证（改进 B）
// 格式: $...*HH — 从 '$' 后到 '*' 前的所有字节异或结果需等于 HH
static bool verifyNmeaChecksum(const char* sentence) {
  if (!sentence || sentence[0] != '$') return false;
  const char* star = strchr(sentence + 1, '*');
  if (!star || star[1] == '\0' || star[2] == '\0') return false;
  uint8_t expected = 0;
  for (const char* p = sentence + 1; p < star; p++) expected ^= (uint8_t)*p;
  char hexBuf[3] = { star[1], star[2], '\0' };
  char* endPtr = nullptr;
  uint8_t got = (uint8_t)strtol(hexBuf, &endPtr, 16);
  if (endPtr != hexBuf + 2) return false;
  return expected == got;
}

// 字段提取辅助（改进 A：不分配 String，直接填充栈上缓冲）
// 返回字段长度；字段不存在时返回 0 并将 out[0] 置 '\0'
static int getFieldBuf(const char* line, int num, char* out, int outSize) {
  if (!out || outSize == 0) return 0;
  out[0] = '\0';
  int field = 0;
  const char* start = line;
  for (const char* p = line; ; p++) {
    char ch = *p;
    if (ch == ',' || ch == '*' || ch == '\0') {
      if (field == num) {
        int len = (int)(p - start);
        if (len >= outSize) len = outSize - 1;
        if (len > 0) memcpy(out, start, len);
        out[len] = '\0';
        return len;
      }
      field++;
      start = p + 1;
      if (ch == '\0' || ch == '*') break;
    }
  }
  return 0;
}

void GPSManager::_nmeaDispatcher(const char* line) {
  if (!line || line[0] == '\0') return;
  if (_nmeaSerial) Serial.println(line);

  // NMEA 句子必须以 '$' 开头
  if (line[0] != '$') return;

  // 校验和验证：拒绝损坏的句子，防止串口噪声写入错误数据
  if (!verifyNmeaChecksum(line)) return;

  // 存入 NMEA 环形缓冲（供监视器页面显示）
  _lastNmeaMillis = millis();
  strncpy(_nmeaBuf[_nmeaBufHead], line, NMEA_BUF_WIDTH - 1);
  _nmeaBuf[_nmeaBufHead][NMEA_BUF_WIDTH - 1] = '\0';
  _nmeaBufHead = (_nmeaBufHead + 1) % NMEA_BUF_LINES;
  if (_nmeaBufCount < NMEA_BUF_LINES) _nmeaBufCount++;

  // 按前缀分发（使用 strncmp，不分配 String）
  struct { const char* prefix; int prefixLen; void (GPSManager::*parser)(const char*); } handlers[] = {
    {"$GPGSV", 6, &GPSManager::_parseGSV},
    {"$GLGSV", 6, &GPSManager::_parseGSV},
    {"$GAGSV", 6, &GPSManager::_parseGSV},
    {"$BDGSV", 6, &GPSManager::_parseGSV},
    {"$GQGSV", 6, &GPSManager::_parseGSV},
    {"$GNGSV", 6, &GPSManager::_parseGSV},
    {"$GPGSA", 6, &GPSManager::_parseGSA},
    {"$GLGSA", 6, &GPSManager::_parseGSA},
    {"$GAGSA", 6, &GPSManager::_parseGSA},
    {"$BDGSA", 6, &GPSManager::_parseGSA},
    {"$GQGSA", 6, &GPSManager::_parseGSA},
    {"$GNGSA", 6, &GPSManager::_parseGSA},
    {"$GPGGA", 6, &GPSManager::_parseGGA},
    {"$GNGGA", 6, &GPSManager::_parseGGA},
  };

  for (auto& h : handlers) {
    if (strncmp(line, h.prefix, h.prefixLen) == 0) {
      (this->*h.parser)(line);
      break;
    }
  }
}

void GPSManager::_parseGSV(const char* line) {
  const char* system;
  if      (strncmp(line, "$GPGSV", 6) == 0) system = "GPS";
  else if (strncmp(line, "$GLGSV", 6) == 0) system = "GLONASS";
  else if (strncmp(line, "$GAGSV", 6) == 0) system = "Galileo";
  else if (strncmp(line, "$BDGSV", 6) == 0) system = "BeiDou";
  else if (strncmp(line, "$GQGSV", 6) == 0) system = "QZSS";
  else if (strncmp(line, "$GNGSV", 6) == 0) system = "Mixed";
  else return;

  GSVSequenceState* state = _getGSVState(system);
  if (!state) return;

  char fbuf[12];
  int totalMsgs = getFieldBuf(line, 1, fbuf, sizeof(fbuf)) ? atoi(fbuf) : 0;
  int msgNum    = getFieldBuf(line, 2, fbuf, sizeof(fbuf)) ? atoi(fbuf) : 0;

  if (msgNum == 1 || state->totalMsgs != totalMsgs) {
    state->currentVisible.clear();
    state->totalMsgs = totalMsgs;
  }

  // NMEA 0183 标准字段顺序（所有星系）: ID, 仰角(elevation), 方位角(azimuth), SNR
  // 注意：BeiDou 遵循相同标准，不存在顺序互换；若模块固件异常请在此处添加特殊分支
  for (int i = 4; ; i += 4) {
    if (!getFieldBuf(line, i, fbuf, sizeof(fbuf)) || fbuf[0] == '\0') break;

    SatData sat;
    sat.system = system;  // Arduino String = const char* 赋值，仅在首次发现卫星时分配
    sat.id = atoi(fbuf);

    char elBuf[8], azBuf[8], snrBuf[8];
    getFieldBuf(line, i + 1, elBuf,  sizeof(elBuf));
    getFieldBuf(line, i + 2, azBuf,  sizeof(azBuf));
    getFieldBuf(line, i + 3, snrBuf, sizeof(snrBuf));

    sat.elevation = elBuf[0]  ? atoi(elBuf)  : 0;
    sat.azimuth   = azBuf[0]  ? atoi(azBuf)  : 0;
    sat.snr       = snrBuf[0] ? atoi(snrBuf) : 0;
    sat.used = false;
    _storeSatellite(sat);
    state->currentVisible.push_back(sat.id);
  }

  state->lastMsgNum = msgNum;
  if (msgNum == totalMsgs) {
    // 本序列完成，更新可见性标志（sat.system 是 String，== const char* 有效）
    for (auto& s : _satellites) {
      if (s.system == system) {
        s.visible = (std::find(state->currentVisible.begin(),
                     state->currentVisible.end(), s.id)
                     != state->currentVisible.end());
      }
    }
  }
}

void GPSManager::_parseGSA(const char* line) {
  // 确定优先星系（用于多星系 ID 冲突时消歧）
  // $GNGSA 表示多星系联合解算，不设优先系统（传空字符串，按 SNR/可见度评分）
  const char* preferredSystem = "";
  if      (strncmp(line, "$GPGSA", 6) == 0) preferredSystem = "GPS";
  else if (strncmp(line, "$GLGSA", 6) == 0) preferredSystem = "GLONASS";
  else if (strncmp(line, "$GAGSA", 6) == 0) preferredSystem = "Galileo";
  else if (strncmp(line, "$BDGSA", 6) == 0) preferredSystem = "BeiDou";
  else if (strncmp(line, "$GQGSA", 6) == 0) preferredSystem = "QZSS";
  // $GNGSA → preferredSystem 保持 ""（正确行为：按可见度+SNR评分）

  char fbuf[16];
  if (!getFieldBuf(line, 2, fbuf, sizeof(fbuf)) || fbuf[0] == '\0') {
    _gsaFixMode = 1;
    _pdop = 99.9f;
    _vdop = 99.9f;
    _lastGsaMillis = 0;
    return;
  }

  _gsaFixMode = atoi(fbuf);

  unsigned long now = millis();
  if (_lastGsaMillis == 0 || now - _lastGsaMillis > 400) {
    _clearUsedFlags();
  }
  _lastGsaMillis = now;

  // 解析参与定位的卫星 ID（NMEA GSA 字段 3-14，共 12 个槽位）
  for (int i = 3; i <= 14; i++) {
    if (getFieldBuf(line, i, fbuf, sizeof(fbuf)) && fbuf[0] != '\0') {
      _markUsedSatellite(preferredSystem, atoi(fbuf));
    }
  }

  // 字段 15: PDOP，字段 16: HDOP（TinyGPSPlus 负责），字段 17: VDOP
  _pdop = getFieldBuf(line, 15, fbuf, sizeof(fbuf)) && fbuf[0] != '\0'
          ? strtof(fbuf, nullptr) : 99.9f;
  _vdop = getFieldBuf(line, 17, fbuf, sizeof(fbuf)) && fbuf[0] != '\0'
          ? strtof(fbuf, nullptr) : 99.9f;
}

void GPSManager::_clearUsedFlags() {
  for (auto& sat : _satellites) sat.used = false;
}

void GPSManager::_markUsedSatellite(const char* preferredSystem, int id) {
  int bestIndex = -1;
  int bestScore = -1;

  for (int i = 0; i < (int)_satellites.size(); i++) {
    const auto& sat = _satellites[i];
    if (sat.id != id) continue;

    int score = 0;
    // sat.system 是 Arduino String，== const char* 有效
    if (preferredSystem && preferredSystem[0] != '\0' && sat.system == preferredSystem)
      score += 100;
    if (sat.visible) score += 10;
    score += constrain(sat.snr, 0, 9);

    if (score > bestScore) {
      bestScore = score;
      bestIndex = i;
    }
  }

  if (bestIndex >= 0) {
    _satellites[bestIndex].used = true;
  }
}

void GPSManager::_parseGGA(const char* line) {
  char fbuf[16];
  if (getFieldBuf(line, 6, fbuf, sizeof(fbuf)) && fbuf[0] != '\0') {
    _ggaFixQuality = atoi(fbuf);
    _lastGgaMillis = millis();
  } else {
    _ggaFixQuality = 0;
    _lastGgaMillis = 0;
  }

  if (getFieldBuf(line, 11, fbuf, sizeof(fbuf)) && fbuf[0] != '\0') {
    _geoidHeight = strtof(fbuf, nullptr);
    _geoidValid = true;
  }
}

GSVSequenceState* GPSManager::_getGSVState(const char* system) {
  for (int i = 0; i < _gsvCount; i++) {
    if (_gsvStates[i].system == system) return &_gsvStates[i];
  }
  if (_gsvCount < 6) {
    _gsvStates[_gsvCount].system = system;
    return &_gsvStates[_gsvCount++];
  }
  return nullptr;
}

void GPSManager::_storeSatellite(const SatData& sat) {
  for (auto& s : _satellites) {
    if (s.system == sat.system && s.id == sat.id) {
      s.elevation = sat.elevation;
      s.azimuth   = sat.azimuth;
      s.snr       = sat.snr;
      return;
    }
  }
  _satellites.push_back(sat);
}

// 清除 60s 无信号后积累的陈旧卫星条目（改进 6）
void GPSManager::purgeStaleSatellites() {
  _satellites.clear();
  for (int i = 0; i < _gsvCount; i++) {
    _gsvStates[i].currentVisible.clear();
    _gsvStates[i].totalMsgs  = 0;
    _gsvStates[i].lastMsgNum = 0;
  }
}

// GPS 省电模式（改进 D）：ATGM336H/CASIC PMTK225 命令
void GPSManager::enablePowerSave(bool enable) {
  if (_powerSaveActive == enable) return;
  _powerSaveActive = enable;
  if (enable) {
    // AlwaysLocate™ 轻度省电：2000ms 全功率采集 + 500ms 低功率守候
    _sendPMTK("PMTK225,2,2000,500");
  } else {
    // 恢复全功率正常模式
    _sendPMTK("PMTK225,0");
  }
}

const char* GPSManager::nmeaLine(int index) const {
  if (index < 0 || index >= _nmeaBufCount) return "";
  int idx = (_nmeaBufHead - _nmeaBufCount + index + NMEA_BUF_LINES) % NMEA_BUF_LINES;
  return _nmeaBuf[idx];
}
