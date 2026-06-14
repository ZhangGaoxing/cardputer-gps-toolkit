/**
 * waypoint_manager.cpp — 航点管理器实现
 *
 * 使用 CSV 格式持久化：
 *   每行一个航点，逗号分隔
 *   文件名: /gpstoolkit/waypoints/waypoints.csv
 *   原子写入: 先写 .tmp，成功后 rename
 *   备份: .bak（如果启用）
 *   容错: 跳过损坏行并记录错误数
 */
#include "waypoint_manager.h"
#include "sd_manager.h"
#include "geo_math.h"
#include "gps_manager.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

namespace {

bool parseIntStrict(const String& text, int& out) {
  if (text.length() == 0) return false;
  char* endPtr = nullptr;
  long value = strtol(text.c_str(), &endPtr, 10);
  if (endPtr == text.c_str() || *endPtr != '\0') return false;
  out = (int)value;
  return true;
}

bool parseUint16Strict(const String& text, uint16_t& out) {
  int value = 0;
  if (!parseIntStrict(text, value) || value < 0 || value > 65535) return false;
  out = (uint16_t)value;
  return true;
}

bool parseUint8Strict(const String& text, uint8_t& out) {
  int value = 0;
  if (!parseIntStrict(text, value) || value < 0 || value > 255) return false;
  out = (uint8_t)value;
  return true;
}

bool parseFloatStrict(const String& text, float& out) {
  if (text.length() == 0) return false;
  char* endPtr = nullptr;
  float value = strtof(text.c_str(), &endPtr);
  if (endPtr == text.c_str() || *endPtr != '\0') return false;
  out = value;
  return true;
}

void updateWaypointTimestamp(Waypoint& wp) {
  GPSManager& gps = GPSManager::instance();
  if (!gps.dateValid() || !gps.timeValid()) return;

  wp.updYear = gps.utcYear();
  wp.updMonth = gps.utcMonth();
  wp.updDay = gps.utcDay();
  wp.updHour = gps.utcHour();
  wp.updMinute = gps.utcMinute();
  wp.updSecond = gps.utcSecond();
}

}

// ==================================================================
//  单例
// ==================================================================

WaypointManager& WaypointManager::instance() {
  static WaypointManager wm;
  return wm;
}

// ==================================================================
//  初始化与加载
// ==================================================================

bool WaypointManager::begin() {
  _sdReady = SDManager::instance().begin();
  _count = 0;
  _nextId = 1;
  _dirty = false;
  _loadErrors = 0;
  memset(_waypoints, 0, sizeof(_waypoints));
  _clearError();

  if (_sdReady) {
    return load();
  }
  // SD 不可用 — 内存模式（无法持久化，但航点操作仍可用）
  _setError("SD not available");
  return true;
}

bool WaypointManager::load() {
  if (!_sdReady) {
    _setError("SD not ready");
    return false;
  }

  _count = 0;
  _nextId = 1;
  _loadErrors = 0;
  _dirty = false;
  memset(_waypoints, 0, sizeof(_waypoints));
  _clearError();

  const char* loadPath = WAYPOINT_FILE_PATH;
  bool recoveredBackup = false;
  File f = SD.open(loadPath, FILE_READ);
  if (!f && SD.exists(WAYPOINT_FILE_PATH ".bak")) {
    loadPath = WAYPOINT_FILE_PATH ".bak";
    f = SD.open(loadPath, FILE_READ);
    recoveredBackup = (bool)f;
  }
  if (!f) {
    // 文件不存在 = 首次启动，不算错误
    return true;
  }

  while (f.available() && _count < WAYPOINT_MAX_COUNT) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    Waypoint wp;
    if (_parseCsvLine(line, wp)) {
      // 检查 ID 是否重复
      bool dup = false;
      for (size_t i = 0; i < _count; i++) {
        if (_waypoints[i].id == wp.id) {
          dup = true;
          break;
        }
      }
      if (!dup) {
        _waypoints[_count++] = wp;
        if (wp.id >= _nextId) {
          _nextId = wp.id + 1;
        }
      } else {
        _loadErrors++;
      }
    } else {
      _loadErrors++;
    }
  }
  f.close();

  if (_loadErrors > 0) {
    _setError("Skipped bad lines");
  } else if (recoveredBackup) {
    _setError("Recovered from backup");
  }
  return true;
}

// ==================================================================
//  持久化保存
// ==================================================================

bool WaypointManager::save() {
  if (!_sdReady) {
    _setError("SD not ready");
    return false;
  }

  // 确保目录存在
  SD.mkdir("/gpstoolkit/waypoints");

  // 原子写入：先写 .tmp
  const char* tmpPath = WAYPOINT_FILE_PATH ".tmp";
  if (SD.exists(tmpPath)) {
    SD.remove(tmpPath);
  }

  File f = SD.open(tmpPath, FILE_WRITE);
  if (!f) {
    _setError("Cannot create file");
    return false;
  }

  String csvLine;
  for (size_t i = 0; i < _count; i++) {
    _formatCsvLine(_waypoints[i], csvLine);
    if (f.print(csvLine) != (int)(csvLine.length())) {
      f.close();
      _setError("Write error");
      return false;
    }
    if (f.print('\n') != 1) {
      f.close();
      _setError("Write error");
      return false;
    }
  }
  f.close();

  bool hadPrimary = SD.exists(WAYPOINT_FILE_PATH);
  if (hadPrimary) {
    if (SD.exists(WAYPOINT_FILE_PATH ".bak") &&
        !SD.remove(WAYPOINT_FILE_PATH ".bak")) {
      _setError("Cannot rotate backup");
      return false;
    }
    if (!SD.rename(WAYPOINT_FILE_PATH, WAYPOINT_FILE_PATH ".bak")) {
      _setError("Cannot rotate current file");
      return false;
    }
  }

  if (!SD.rename(tmpPath, WAYPOINT_FILE_PATH)) {
    if (hadPrimary) {
      SD.rename(WAYPOINT_FILE_PATH ".bak", WAYPOINT_FILE_PATH);
    }
    _setError("Rename failed (old data in .bak)");
    return false;
  }

  _dirty = false;
  _clearError();
  return true;
}

bool WaypointManager::_autoSave() {
#if WAYPOINT_AUTO_SAVE
  if (_dirty && _sdReady) {
    return save();
  }
#endif
  return true;
}

bool WaypointManager::_rollbackFailedSave() {
  char saveError[sizeof(_lastError)];
  strncpy(saveError, _lastError, sizeof(saveError) - 1);
  saveError[sizeof(saveError) - 1] = '\0';

  if (_sdReady && load()) {
    _setError(saveError);
    return false;
  }

  _setError("Save failed; reload failed");
  return false;
}

// ==================================================================
//  CRUD 操作
// ==================================================================

const Waypoint* WaypointManager::addWaypoint(
    const char* name, float lat, float lon, float ele,
    WaypointSource source, WaypointType type, const char* note,
    int year, int month, int day, int hour, int minute, int second) {

  if (_count >= WAYPOINT_MAX_COUNT) {
    _setError("Waypoint list full");
    return nullptr;
  }

  // 验证坐标范围
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    _setError("Invalid coordinates");
    return nullptr;
  }

  // 验证名称
  if (name == nullptr || name[0] == '\0') {
    _setError("Name required");
    return nullptr;
  }

  Waypoint& wp = _waypoints[_count];
  memset(&wp, 0, sizeof(Waypoint));

  wp.id = _nextId++;

  strncpy(wp.name, name, WAYPOINT_NAME_MAX_LEN - 1);
  wp.name[WAYPOINT_NAME_MAX_LEN - 1] = '\0';

  wp.lat = lat;
  wp.lon = lon;
  wp.ele = ele;
  wp.source = (uint8_t)source;
  wp.type = (uint8_t)type;

  if (note != nullptr) {
    strncpy(wp.note, note, WAYPOINT_NOTE_MAX_LEN - 1);
    wp.note[WAYPOINT_NOTE_MAX_LEN - 1] = '\0';
  } else {
    wp.note[0] = '\0';
  }

  // 设置创建时间和更新时间
  wp.year   = year;
  wp.month  = month;
  wp.day    = day;
  wp.hour   = hour;
  wp.minute = minute;
  wp.second = second;
  wp.updYear   = year;
  wp.updMonth  = month;
  wp.updDay    = day;
  wp.updHour   = hour;
  wp.updMinute = minute;
  wp.updSecond = second;

  _count++;
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    _rollbackFailedSave();
    return nullptr;
  }

  return &_waypoints[_count - 1];
}

bool WaypointManager::deleteWaypoint(uint16_t id) {
  int idx = _findIndexById(id);
  if (idx < 0) {
    _setError("Not found");
    return false;
  }

  // 移动后续元素
  for (size_t i = idx; i < _count - 1; i++) {
    _waypoints[i] = _waypoints[i + 1];
  }
  // 清零尾部
  memset(&_waypoints[_count - 1], 0, sizeof(Waypoint));
  _count--;
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    return _rollbackFailedSave();
  }
  return true;
}

bool WaypointManager::renameWaypoint(uint16_t id, const char* newName) {
  if (newName == nullptr || newName[0] == '\0') {
    _setError("Name required");
    return false;
  }

  int idx = _findIndexById(id);
  if (idx < 0) {
    _setError("Not found");
    return false;
  }

  Waypoint& wp = _waypoints[idx];
  strncpy(wp.name, newName, WAYPOINT_NAME_MAX_LEN - 1);
  wp.name[WAYPOINT_NAME_MAX_LEN - 1] = '\0';
  updateWaypointTimestamp(wp);
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    return _rollbackFailedSave();
  }
  return true;
}

bool WaypointManager::updateWaypoint(
    uint16_t id, const char* name, float lat, float lon, float ele,
    WaypointSource source, WaypointType type, const char* note) {
  int idx = _findIndexById(id);
  if (idx < 0) {
    _setError("Not found");
    return false;
  }

  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    _setError("Invalid coordinates");
    return false;
  }

  Waypoint& wp = _waypoints[idx];
  strncpy(wp.name, name != nullptr ? name : "", WAYPOINT_NAME_MAX_LEN - 1);
  wp.name[WAYPOINT_NAME_MAX_LEN - 1] = '\0';
  wp.lat = lat;
  wp.lon = lon;
  wp.ele = ele;
  wp.source = (uint8_t)source;
  wp.type = (uint8_t)type;
  if (note != nullptr) {
    strncpy(wp.note, note, WAYPOINT_NOTE_MAX_LEN - 1);
    wp.note[WAYPOINT_NOTE_MAX_LEN - 1] = '\0';
  } else {
    wp.note[0] = '\0';
  }
  updateWaypointTimestamp(wp);
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    return _rollbackFailedSave();
  }
  return true;
}

bool WaypointManager::updateNote(uint16_t id, const char* newNote) {
  int idx = _findIndexById(id);
  if (idx < 0) {
    _setError("Not found");
    return false;
  }
  Waypoint& wp = _waypoints[idx];
  strncpy(wp.note, newNote != nullptr ? newNote : "", WAYPOINT_NOTE_MAX_LEN - 1);
  wp.note[WAYPOINT_NOTE_MAX_LEN - 1] = '\0';
  updateWaypointTimestamp(wp);
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    return _rollbackFailedSave();
  }
  return true;
}

bool WaypointManager::updateType(uint16_t id, WaypointType newType) {
  int idx = _findIndexById(id);
  if (idx < 0) {
    _setError("Not found");
    return false;
  }
  _waypoints[idx].type = (uint8_t)newType;
  updateWaypointTimestamp(_waypoints[idx]);
  _dirty = true;
  _clearError();
  if (!_autoSave()) {
    return _rollbackFailedSave();
  }
  return true;
}

// ==================================================================
//  查询操作
// ==================================================================

const Waypoint* WaypointManager::getByIndex(size_t index) const {
  if (index >= _count) return nullptr;
  return &_waypoints[index];
}

const Waypoint* WaypointManager::getById(uint16_t id) const {
  int idx = _findIndexById(id);
  if (idx < 0) return nullptr;
  return &_waypoints[idx];
}

int WaypointManager::findNearest(float lat, float lon) const {
  if (_count == 0) return -1;
  int bestIdx = -1;
  float bestDist = 1e30f;
  for (size_t i = 0; i < _count; i++) {
    float d = distanceKm(lat, lon, _waypoints[i].lat, _waypoints[i].lon);
    if (d < bestDist) {
      bestDist = d;
      bestIdx = (int)i;
    }
  }
  return bestIdx;
}

float WaypointManager::distanceToWaypoint(uint16_t id, float currentLat, float currentLon) const {
  const Waypoint* wp = getById(id);
  if (!wp) return -1.0f;
  return distanceKm(currentLat, currentLon, wp->lat, wp->lon);
}

float WaypointManager::bearingToWaypoint(uint16_t id, float currentLat, float currentLon) const {
  const Waypoint* wp = getById(id);
  if (!wp) return -1.0f;
  return bearingDeg(currentLat, currentLon, wp->lat, wp->lon);
}

// ==================================================================
//  导航计算工具
// ==================================================================

float WaypointManager::distanceKm(float lat1, float lon1, float lat2, float lon2) {
  return geoDistanceKm(lat1, lon1, lat2, lon2);
}

float WaypointManager::bearingDeg(float lat1, float lon1, float lat2, float lon2) {
  return geoBearingDeg(lat1, lon1, lat2, lon2);
}

// ==================================================================
//  内部辅助
// ==================================================================

void WaypointManager::_setError(const char* msg) {
  strncpy(_lastError, msg, sizeof(_lastError) - 1);
  _lastError[sizeof(_lastError) - 1] = '\0';
}

void WaypointManager::_clearError() {
  _lastError[0] = '\0';
}

int WaypointManager::_findIndexById(uint16_t id) const {
  for (size_t i = 0; i < _count; i++) {
    if (_waypoints[i].id == id) return (int)i;
  }
  return -1;
}

// ==================================================================
//  CSV 解析和格式化
// ==================================================================

bool WaypointManager::_parseCsvLine(const String& line, Waypoint& wp) {
  // 格式: id,name,lat,lon,ele,year,month,day,hour,minute,second,
  //        updYear,updMonth,updDay,updHour,updMinute,updSecond,
  //        source,type,note
  //
  // 使用手动解析，避免 String split 的大量分配

  memset(&wp, 0, sizeof(Waypoint));
  String fields[20];
  int field = 0;
  bool inQuotes = false;

  for (int i = 0; i < (int)line.length(); i++) {
    char c = line[i];
    if (c == '"') {
      if (inQuotes && i + 1 < (int)line.length() && line[i + 1] == '"') {
        fields[field] += '"';
        i++;
      } else {
        inQuotes = !inQuotes;
      }
      continue;
    }

    if (c == ',' && !inQuotes) {
      if (++field >= 20) return false;
      continue;
    }

    fields[field] += c;
  }

  if (inQuotes || field != 19) return false;

  fields[1].trim();
  fields[19].trim();
  for (int i = 0; i < 20; i++) {
    if (i != 1 && i != 19) fields[i].trim();
  }

  if (!parseUint16Strict(fields[0], wp.id)) return false;
  _csvUnescape(fields[1]);
  strncpy(wp.name, fields[1].c_str(), WAYPOINT_NAME_MAX_LEN - 1);
  wp.name[WAYPOINT_NAME_MAX_LEN - 1] = '\0';
  if (!parseFloatStrict(fields[2], wp.lat)) return false;
  if (!parseFloatStrict(fields[3], wp.lon)) return false;
  if (!parseFloatStrict(fields[4], wp.ele)) return false;
  if (!parseIntStrict(fields[5], wp.year)) return false;
  if (!parseIntStrict(fields[6], wp.month)) return false;
  if (!parseIntStrict(fields[7], wp.day)) return false;
  if (!parseIntStrict(fields[8], wp.hour)) return false;
  if (!parseIntStrict(fields[9], wp.minute)) return false;
  if (!parseIntStrict(fields[10], wp.second)) return false;
  if (!parseIntStrict(fields[11], wp.updYear)) return false;
  if (!parseIntStrict(fields[12], wp.updMonth)) return false;
  if (!parseIntStrict(fields[13], wp.updDay)) return false;
  if (!parseIntStrict(fields[14], wp.updHour)) return false;
  if (!parseIntStrict(fields[15], wp.updMinute)) return false;
  if (!parseIntStrict(fields[16], wp.updSecond)) return false;
  if (!parseUint8Strict(fields[17], wp.source)) return false;
  if (!parseUint8Strict(fields[18], wp.type)) return false;
  _csvUnescape(fields[19]);
  strncpy(wp.note, fields[19].c_str(), WAYPOINT_NOTE_MAX_LEN - 1);
  wp.note[WAYPOINT_NOTE_MAX_LEN - 1] = '\0';

  // 验证关键字段
  if (wp.id == 0) return false;
  if (wp.name[0] == '\0') return false;
  if (wp.lat < -90.0f || wp.lat > 90.0f) return false;
  if (wp.lon < -180.0f || wp.lon > 180.0f) return false;

  return true;
}

void WaypointManager::_formatCsvLine(const Waypoint& wp, String& out) {
  char buf[32];
  String escName = _csvEscape(wp.name);
  String escNote = _csvEscape(wp.note);

  out = "";
  out += String(wp.id);
  out += ',';
  out += escName;
  out += ',';

  snprintf(buf, sizeof(buf), "%.6f,%.6f,%.1f", wp.lat, wp.lon, wp.ele);
  out += buf;
  out += ',';

  snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d",
           wp.year, wp.month, wp.day, wp.hour, wp.minute, wp.second);
  out += buf;
  out += ',';

  snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d",
           wp.updYear, wp.updMonth, wp.updDay, wp.updHour, wp.updMinute, wp.updSecond);
  out += buf;
  out += ',';

  snprintf(buf, sizeof(buf), "%d,%d", (int)wp.source, (int)wp.type);
  out += buf;
  out += ',';

  // note 用引号包裹（防止换行或逗号造成解析错误）
  out += '"';
  out += escNote;
  out += '"';
}

// ==================================================================
//  CSV 转义
// ==================================================================

String WaypointManager::_csvEscape(const char* str) {
  // 允许逗号；双引号按 CSV 规则转义；换行仍剔除。
  String out = "";
  while (*str) {
    char c = *str++;
    if (c == '"') {
      // 双引号 → 双引号转义
      out += '"';
    }
    if (c == '\r' || c == '\n') {
      // 不允许换行
      continue;
    }
    out += c;
  }
  return out;
}

void WaypointManager::_csvUnescape(String& str) {
  // 将双引号转义还原
  str.replace("\"\"", "\"");
}
