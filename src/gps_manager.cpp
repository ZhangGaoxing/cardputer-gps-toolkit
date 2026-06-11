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
}

void GPSManager::_closeSerial() {
  _serial.end();
}

// ==================================================================
//  主更新循环
// ==================================================================

void GPSManager::update() {
  static String nmeaLine = "";
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
      _nmeaDispatcher(nmeaLine);
      nmeaLine = "";
    } else if (c != '\r') {
      nmeaLine += c;
    }
  }

  if (gotValidChar) {
    _lastValidGpsMillis = millis();
    _gpsState = GPS_ON;
  }
}

// ==================================================================
//  NMEA分发和解析
// ==================================================================

void GPSManager::_nmeaDispatcher(const String& line) {
  if (_nmeaSerial) Serial.println(line);

  String trimmed = line;
  trimmed.trim();

  // 存入NMEA环形缓冲
  if (trimmed.length() > 0 && trimmed[0] == '$') {
    strncpy(_nmeaBuf[_nmeaBufHead], trimmed.c_str(), NMEA_BUF_WIDTH - 1);
    _nmeaBuf[_nmeaBufHead][NMEA_BUF_WIDTH - 1] = '\0';
    _nmeaBufHead = (_nmeaBufHead + 1) % NMEA_BUF_LINES;
    if (_nmeaBufCount < NMEA_BUF_LINES) _nmeaBufCount++;
  }

  // 按前缀分发到对应解析器
  struct { const char* prefix; void (GPSManager::*parser)(const String&); } handlers[] = {
    {"$GPGSV", &GPSManager::_parseGSV},
    {"$GLGSV", &GPSManager::_parseGSV},
    {"$GAGSV", &GPSManager::_parseGSV},
    {"$BDGSV", &GPSManager::_parseGSV},
    {"$GQGSV", &GPSManager::_parseGSV},
    {"$GNGSV", &GPSManager::_parseGSV},
    {"$GPGSA", &GPSManager::_parseGSA},
    {"$GLGSA", &GPSManager::_parseGSA},
    {"$GAGSA", &GPSManager::_parseGSA},
    {"$BDGSA", &GPSManager::_parseGSA},
    {"$GQGSA", &GPSManager::_parseGSA},
    {"$GNGSA", &GPSManager::_parseGSA},
    {"$GPGGA", &GPSManager::_parseGGA},
    {"$GNGGA", &GPSManager::_parseGGA},
  };

  for (auto& h : handlers) {
    if (line.startsWith(h.prefix)) {
      (this->*h.parser)(line);
      break;
    }
  }
}

// 字符串字段提取辅助函数
static String getField(const String& line, int num) {
  int field = 0, start = 0;
  for (int i = 0; i <= (int)line.length(); i++) {
    if (i == (int)line.length() || line[i] == ',' || line[i] == '*') {
      if (field == num) return line.substring(start, i);
      field++;
      start = i + 1;
    }
  }
  return "";
}

void GPSManager::_parseGSV(const String& line) {
  String system;
  if (line.startsWith("$GPGSV")) system = "GPS";
  else if (line.startsWith("$GLGSV")) system = "GLONASS";
  else if (line.startsWith("$GAGSV")) system = "Galileo";
  else if (line.startsWith("$BDGSV")) system = "BeiDou";
  else if (line.startsWith("$GQGSV")) system = "QZSS";
  else if (line.startsWith("$GNGSV")) system = "Mixed";
  else return;

  GSVSequenceState* state = _getGSVState(system);
  if (!state) return;

  int totalMsgs = getField(line, 1).toInt();
  int msgNum    = getField(line, 2).toInt();

  if (msgNum == 1 || state->totalMsgs != totalMsgs) {
    state->currentVisible.clear();
    state->totalMsgs = totalMsgs;
  }

  // 解析卫星数据（每组4个字段：ID, 仰角, 方位角, SNR）
  for (int i = 4; ; i += 4) {
    String idStr = getField(line, i);
    if (idStr.length() == 0) break;

    SatData sat;
    sat.system = system;
    sat.id = idStr.toInt();

    if (system == "BeiDou") {
      // BeiDou方位角和仰角顺序相反
      sat.azimuth   = getField(line, i + 1).toInt();
      sat.elevation = getField(line, i + 2).toInt();
    } else {
      sat.elevation = getField(line, i + 1).toInt();
      sat.azimuth   = getField(line, i + 2).toInt();
    }
    sat.snr = getField(line, i + 3).toInt();
    sat.used = false;
    _storeSatellite(sat);
    state->currentVisible.push_back(sat.id);
  }

  state->lastMsgNum = msgNum;
  if (msgNum == totalMsgs) {
    // 本序列完成，更新可见性标志
    for (auto& s : _satellites) {
      if (s.system == system) {
        s.visible = (std::find(state->currentVisible.begin(),
                     state->currentVisible.end(), s.id)
                     != state->currentVisible.end());
      }
    }
  }
}

void GPSManager::_parseGSA(const String& line) {
  String preferredSystem;
  if (line.startsWith("$GPGSA")) preferredSystem = "GPS";
  else if (line.startsWith("$GLGSA")) preferredSystem = "GLONASS";
  else if (line.startsWith("$GAGSA")) preferredSystem = "Galileo";
  else if (line.startsWith("$BDGSA")) preferredSystem = "BeiDou";
  else if (line.startsWith("$GQGSA")) preferredSystem = "QZSS";

  String f2 = getField(line, 2);
  if (f2.length() > 0) _gsaFixMode = f2.toInt();

  unsigned long now = millis();
  if (_lastGsaMillis == 0 || now - _lastGsaMillis > 400) {
    _clearUsedFlags();
  }
  _lastGsaMillis = now;

  // 解析参与定位的卫星ID（字段3-14）
  for (int i = 3; i <= 14; i++) {
    String sid = getField(line, i);
    if (sid.length() > 0) {
      int id = sid.toInt();
      _markUsedSatellite(preferredSystem, id);
    }
  }

  String fp = getField(line, 15);
  if (fp.length() > 0) _pdop = fp.toFloat();

  String fv = getField(line, 17);
  if (fv.length() > 0) _vdop = fv.toFloat();
}

void GPSManager::_clearUsedFlags() {
  for (auto& sat : _satellites) sat.used = false;
}

void GPSManager::_markUsedSatellite(const String& preferredSystem, int id) {
  int bestIndex = -1;
  int bestScore = -1;

  for (int i = 0; i < (int)_satellites.size(); i++) {
    const auto& sat = _satellites[i];
    if (sat.id != id) continue;

    int score = 0;
    if (preferredSystem.length() > 0 && sat.system == preferredSystem) score += 100;
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

void GPSManager::_parseGGA(const String& line) {
  String f6 = getField(line, 6);
  if (f6.length() > 0) _ggaFixQuality = f6.toInt();

  String f11 = getField(line, 11);
  if (f11.length() > 0) {
    _geoidHeight = f11.toFloat();
    _geoidValid = true;
  }
}

GSVSequenceState* GPSManager::_getGSVState(const String& system) {
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

const char* GPSManager::nmeaLine(int index) const {
  if (index < 0 || index >= _nmeaBufCount) return "";
  int idx = (_nmeaBufHead - _nmeaBufCount + index + NMEA_BUF_LINES) % NMEA_BUF_LINES;
  return _nmeaBuf[idx];
}
