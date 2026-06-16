/**
 * backtrack_manager.cpp - frozen-track backtrack navigation state.
 */
#include "backtrack_manager.h"
#include "gps_manager.h"
#include "navigation_manager.h"
#include "geo_math.h"
#include "sd_manager.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

namespace {
const float TURN_KEEP_ANGLE_DEG = 28.0f;
const float TURN_KEEP_MIN_LEG_M = 4.0f;

bool endsWithIgnoreCase(const char* value, const char* suffix) {
  if (!value || !suffix) return false;
  size_t valueLen = strlen(value);
  size_t suffixLen = strlen(suffix);
  if (valueLen < suffixLen) return false;
  const char* tail = value + valueLen - suffixLen;
  for (size_t i = 0; i < suffixLen; i++) {
    char a = tail[i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

bool parseAttributeFloat(const char* text, const char* attr, float& out) {
  if (!text || !attr) return false;
  const char* p = strstr(text, attr);
  if (!p) return false;
  p += strlen(attr);
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '=') return false;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  char quote = *p;
  if (quote != '"' && quote != '\'') return false;
  p++;

  char buf[24];
  size_t len = 0;
  while (*p && *p != quote && len < sizeof(buf) - 1) {
    buf[len++] = *p++;
  }
  buf[len] = '\0';
  if (*p != quote || len == 0) return false;

  char* endPtr = nullptr;
  float value = strtof(buf, &endPtr);
  if (endPtr == buf || *endPtr != '\0') return false;
  out = value;
  return true;
}

bool parseGpxTrackPointLine(const char* line, TrackPoint& out, bool segmentStart) {
  if (!line || !strstr(line, "<trkpt")) return false;
  float lat = NAN;
  float lon = NAN;
  if (!parseAttributeFloat(line, "lat", lat) ||
      !parseAttributeFloat(line, "lon", lon)) {
    return false;
  }
  out = TrackPoint{lat, lon, 0.0f, 0.0f, millis(), segmentStart};
  return true;
}

bool readTrimmedLine(File& file, char* out, size_t outSize) {
  if (!out || outSize == 0 || !file.available()) return false;

  size_t len = 0;
  bool truncated = false;
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    if (c == '\n') break;
    if (c == '\r') continue;
    if (len < outSize - 1) {
      out[len++] = (char)c;
    } else {
      truncated = true;
    }
  }

  if (truncated) {
    out[0] = '\0';
    return true;
  }

  out[len] = '\0';
  char* start = out;
  while (*start == ' ' || *start == '\t') start++;

  char* end = start + strlen(start);
  while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
    end--;
  }
  *end = '\0';

  if (start != out) {
    memmove(out, start, strlen(start) + 1);
  }
  return true;
}
}

BacktrackManager& BacktrackManager::instance() {
  static BacktrackManager bt;
  return bt;
}

void BacktrackManager::begin() {
  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  _pointCount = 0;
  memset(_points, 0, sizeof(_points));
  memset(_cumDistanceM, 0, sizeof(_cumDistanceM));
  _lastRecalcMs = 0;
}

bool BacktrackManager::startFromCurrentTrip(BacktrackTargetMode mode) {
  if (!_buildSnapshotFromTrip()) {
    return false;
  }

  if (NavigationManager::instance().isActive()) {
    NavigationManager::instance().stop("Backtrack active");
  }

  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  _state.active = true;
  _state.source = BacktrackSource::CurrentTrip;
  _state.targetMode = mode;
  _state.targetIndex = (_pointCount > 0) ? _pointCount - 1 : 0;
  _lastRecalcMs = 0;
  _setError("");
  update();
  return true;
}

bool BacktrackManager::loadFromGpx(const char* path) {
  if (_state.active) {
    _setError("Stop Backtrack first");
    return false;
  }
  if (!path || path[0] == '\0') {
    _setError("No GPX path");
    return false;
  }
  if (!endsWithIgnoreCase(path, ".gpx")) {
    _setError("Not a GPX file");
    return false;
  }
  if (!SDManager::instance().begin()) {
    _setError("SD not ready");
    return false;
  }

  const float tries[] = {
    BACKTRACK_MIN_POINT_DISTANCE_M,
    BACKTRACK_MIN_POINT_DISTANCE_M * 1.5f,
    BACKTRACK_MIN_POINT_DISTANCE_M * 2.0f,
    BACKTRACK_MIN_POINT_DISTANCE_M * 3.0f,
    BACKTRACK_MIN_POINT_DISTANCE_M * 5.0f
  };

  size_t parsedPoints = 0;
  bool loaded = false;
  for (size_t i = 0; i < sizeof(tries) / sizeof(tries[0]); i++) {
    parsedPoints = 0;
    if (_loadGpxSimplified(path, tries[i], parsedPoints)) {
      loaded = true;
      break;
    }
  }

  if (!loaded) {
    if (parsedPoints == 0) _setError("No GPX points");
    else if (_pointCount < BACKTRACK_MIN_POINTS_TO_START) _setError("GPX track short");
    else _setError("GPX too dense");
    _pointCount = 0;
    memset(_points, 0, sizeof(_points));
    memset(_cumDistanceM, 0, sizeof(_cumDistanceM));
    return false;
  }

  if (NavigationManager::instance().isActive()) {
    NavigationManager::instance().stop("Backtrack active");
  }

  _computeCumulativeDistance();
  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  _state.active = true;
  _state.source = BacktrackSource::SavedGpx;
  _state.targetMode = BacktrackTargetMode::ReturnToStart;
  _state.targetIndex = (_pointCount > 0) ? _pointCount - 1 : 0;
  _lastRecalcMs = 0;
  _setError("");
  update();
  return true;
}

void BacktrackManager::stop(const char* reason) {
  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  _pointCount = 0;
  memset(_points, 0, sizeof(_points));
  memset(_cumDistanceM, 0, sizeof(_cumDistanceM));
  if (reason && reason[0] != '\0') {
    _setError(reason);
  }
}

void BacktrackManager::update() {
  if (!_state.active) return;

  unsigned long now = millis();
  if (_lastRecalcMs != 0 && now - _lastRecalcMs < BACKTRACK_RECALC_INTERVAL_MS) {
    return;
  }
  _lastRecalcMs = now;

  if (_pointCount < BACKTRACK_MIN_POINTS_TO_START) {
    stop("Track cleared");
    return;
  }
  if (!_validIndex(_state.targetIndex)) {
    _state.targetIndex = _pointCount - 1;
    _setError("Target index reset");
  }

  GPSManager& gps = GPSManager::instance();
  if (!gps.hasReliableFix()) {
    _clearComputed();
    _state.active = true;
    _setError("No reliable fix");
    return;
  }

  float lat = gps.latitude();
  float lon = gps.longitude();
  if (!_isValidCoordinate(lat, lon)) {
    _clearComputed();
    _state.active = true;
    _setError("Invalid GPS");
    return;
  }

  float nearestPointDistanceM = NAN;
  size_t nearestIndex = _findNearestPoint(lat, lon, nearestPointDistanceM);
  float routeDistanceM = _distanceToRouteM(lat, lon);
  bool canJudgeOffTrack = gps.hdop() <= TRACK_MAX_HDOP &&
                          gps.satellitesUsed() >= TRACK_MIN_SATELLITES;
  bool offTrack = canJudgeOffTrack &&
                  isfinite(routeDistanceM) &&
                  routeDistanceM > BACKTRACK_OFF_ROUTE_DISTANCE_M;

  size_t targetIndex = offTrack ? nearestIndex : _chooseTargetIndex(nearestIndex);
  if (!_validIndex(targetIndex)) {
    targetIndex = 0;
    _setError("Target index reset");
  }

  TrackPoint target = _points[targetIndex];
  TrackPoint start = _points[0];

  _state.targetIndex = targetIndex;
  _state.nearestIndex = nearestIndex;
  _state.nearestRouteDistanceM = routeDistanceM;
  _state.offTrack = offTrack;
  _state.distanceToTargetM = geoDistanceKm(lat, lon, target.lat, target.lon) * 1000.0f;
  _state.distanceToStartM = geoDistanceKm(lat, lon, start.lat, start.lon) * 1000.0f;
  _state.remainingTrackDistanceM =
    _remainingFromTargetM(targetIndex, _state.distanceToTargetM);
  _state.bearingDeg = geoBearingDeg(lat, lon, target.lat, target.lon);
  _state.dataAvailable = isfinite(_state.distanceToTargetM) &&
                         isfinite(_state.distanceToStartM) &&
                         isfinite(_state.remainingTrackDistanceM) &&
                         isfinite(_state.bearingDeg);

  bool headingValid = gps.courseValid() && gps.speedValid() && gps.speedMps() >= 0.8f;
  if (headingValid) {
    _state.headingDeg = _normalize360(gps.courseDeg());
    _state.relativeBearingDeg = _normalizeRelative(_state.bearingDeg - _state.headingDeg);
  } else {
    _state.headingDeg = NAN;
    _state.relativeBearingDeg = NAN;
  }

  _state.arrived = _state.dataAvailable &&
                   _state.distanceToStartM <= BACKTRACK_ARRIVAL_RADIUS_M;
  if (_state.arrived) {
    _state.targetIndex = 0;
    _state.distanceToTargetM = _state.distanceToStartM;
    _state.remainingTrackDistanceM = _state.distanceToStartM;
  }

  if (_state.arrived) {
    _setError("");
  } else if (_state.offTrack) {
    _setError("Off Track");
  } else {
    _setError("");
  }
}

TrackPoint BacktrackManager::pointAt(size_t index) const {
  if (index >= _pointCount) return TrackPoint{0, 0, 0, 0, 0, false};
  return _points[index];
}

bool BacktrackManager::targetPoint(TrackPoint& out) const {
  if (!_state.active || !_validIndex(_state.targetIndex)) return false;
  out = _points[_state.targetIndex];
  return true;
}

bool BacktrackManager::startPoint(TrackPoint& out) const {
  if (!_state.active || _pointCount == 0) return false;
  out = _points[0];
  return true;
}

const char* BacktrackManager::sourceText(BacktrackSource source) {
  switch (source) {
    case BacktrackSource::CurrentTrip: return "Current Trip";
    case BacktrackSource::SavedGpx: return "Saved GPX";
    case BacktrackSource::SimplifiedTrack: return "Simplified";
    default: return "Unknown";
  }
}

const char* BacktrackManager::modeText(BacktrackTargetMode mode) {
  switch (mode) {
    case BacktrackTargetMode::ReturnToStart: return "To Start";
    case BacktrackTargetMode::ReturnPreviousPoint: return "Prev Point";
    case BacktrackTargetMode::ReturnNearestPoint: return "Nearest";
    default: return "Unknown";
  }
}

const char* BacktrackManager::relativeText(float relDeg, bool headingValid) {
  if (!headingValid || !isfinite(relDeg)) return "--";
  float a = fabsf(relDeg);
  if (a <= 15.0f) return "Ahead";
  if (a >= 150.0f) return "Behind";
  if (a >= 35.0f) return relDeg > 0.0f ? "Turn R" : "Turn L";
  return relDeg > 0.0f ? "Right" : "Left";
}

bool BacktrackManager::_buildSnapshotFromTrip() {
  TripTracker& trip = TripTracker::instance();
  if (trip.pointCount() < BACKTRACK_MIN_POINTS_TO_START) {
    _setError("Not enough track");
    return false;
  }

  const float tries[] = {
    BACKTRACK_MIN_POINT_DISTANCE_M,
    BACKTRACK_MIN_POINT_DISTANCE_M * 1.5f,
    BACKTRACK_MIN_POINT_DISTANCE_M * 2.0f,
    BACKTRACK_MIN_POINT_DISTANCE_M * 3.0f
  };

  for (size_t i = 0; i < sizeof(tries) / sizeof(tries[0]); i++) {
    if (_copySimplified(tries[i])) {
      if (_pointCount >= BACKTRACK_MIN_POINTS_TO_START) {
        _computeCumulativeDistance();
        return true;
      }
    }
  }

  _setError(_pointCount < BACKTRACK_MIN_POINTS_TO_START ? "Track too short"
                                                        : "Snapshot full");
  return false;
}

bool BacktrackManager::_copySimplified(float minDistanceM) {
  TripTracker& trip = TripTracker::instance();
  size_t total = trip.pointCount();
  _pointCount = 0;
  memset(_points, 0, sizeof(_points));
  memset(_cumDistanceM, 0, sizeof(_cumDistanceM));

  if (total == 0) return false;
  if (!_appendPoint(trip.pointAt(0))) return false;

  for (size_t i = 1; i + 1 < total; i++) {
    TrackPoint prev = trip.pointAt(i - 1);
    TrackPoint cur = trip.pointAt(i);
    TrackPoint next = trip.pointAt(i + 1);
    TrackPoint lastKept = _points[_pointCount - 1];

    float distFromLastM = geoDistanceKm(lastKept.lat, lastKept.lon,
                                        cur.lat, cur.lon) * 1000.0f;
    bool keep = cur.segmentStart ||
                next.segmentStart ||
                distFromLastM >= minDistanceM ||
                _shouldKeepTurn(prev, cur, next);
    if (keep && !_appendPoint(cur)) {
      return false;
    }
  }

  TrackPoint last = trip.pointAt(total - 1);
  if (_pointCount == 0 ||
      _points[_pointCount - 1].lat != last.lat ||
      _points[_pointCount - 1].lon != last.lon ||
      _points[_pointCount - 1].timestamp != last.timestamp) {
    if (!_appendPoint(last)) return false;
  }
  return true;
}

bool BacktrackManager::_loadGpxSimplified(const char* path, float minDistanceM,
                                          size_t& parsedPoints) {
  parsedPoints = 0;
  _pointCount = 0;
  memset(_points, 0, sizeof(_points));
  memset(_cumDistanceM, 0, sizeof(_cumDistanceM));

  File file = SD.open(path, FILE_READ);
  if (!file) {
    _setError("GPX open failed");
    return false;
  }

  bool nextSegmentStart = true;
  bool hasPending = false;
  TrackPoint pending{0, 0, 0, 0, 0, false};
  bool overflow = false;
  char line[192];

  while (readTrimmedLine(file, line, sizeof(line))) {
    if (strstr(line, "<trkseg")) {
      nextSegmentStart = true;
      continue;
    }

    TrackPoint point;
    if (!parseGpxTrackPointLine(line, point, nextSegmentStart)) {
      continue;
    }

    parsedPoints++;
    nextSegmentStart = false;
    if (!_appendGpxCandidate(point, pending, hasPending, minDistanceM)) {
      overflow = true;
      break;
    }
    yield();
  }
  file.close();

  if (!overflow && hasPending) {
    if (!_appendPoint(pending)) overflow = true;
    hasPending = false;
  }

  return !overflow && parsedPoints > 0 && _pointCount >= BACKTRACK_MIN_POINTS_TO_START;
}

bool BacktrackManager::_appendPoint(const TrackPoint& point) {
  if (_pointCount >= BACKTRACK_MAX_POINTS) return false;
  if (!_isValidCoordinate(point.lat, point.lon)) return true;
  _points[_pointCount++] = point;
  return true;
}

bool BacktrackManager::_appendGpxCandidate(const TrackPoint& point,
                                           TrackPoint& pending,
                                           bool& hasPending,
                                           float minDistanceM) {
  if (!_isValidCoordinate(point.lat, point.lon)) return true;

  if (_pointCount == 0) {
    TrackPoint first = point;
    first.segmentStart = true;
    return _appendPoint(first);
  }

  if (point.segmentStart) {
    if (hasPending && !_appendPoint(pending)) return false;
    hasPending = false;
    return _appendPoint(point);
  }

  if (!hasPending) {
    pending = point;
    hasPending = true;
    return true;
  }

  TrackPoint lastKept = _points[_pointCount - 1];
  float distFromLastM = geoDistanceKm(lastKept.lat, lastKept.lon,
                                      pending.lat, pending.lon) * 1000.0f;
  bool keep = distFromLastM >= minDistanceM ||
              _shouldKeepTurn(lastKept, pending, point);
  if (keep && !_appendPoint(pending)) {
    return false;
  }
  pending = point;
  hasPending = true;
  return true;
}

bool BacktrackManager::_shouldKeepTurn(const TrackPoint& prev, const TrackPoint& cur,
                                       const TrackPoint& next) const {
  if (cur.segmentStart || next.segmentStart) return true;
  float d1 = geoDistanceKm(prev.lat, prev.lon, cur.lat, cur.lon) * 1000.0f;
  float d2 = geoDistanceKm(cur.lat, cur.lon, next.lat, next.lon) * 1000.0f;
  if (d1 < TURN_KEEP_MIN_LEG_M || d2 < TURN_KEEP_MIN_LEG_M) return false;

  float b1 = geoBearingDeg(prev.lat, prev.lon, cur.lat, cur.lon);
  float b2 = geoBearingDeg(cur.lat, cur.lon, next.lat, next.lon);
  float delta = fabsf(_normalizeRelative(b2 - b1));
  return isfinite(delta) && delta >= TURN_KEEP_ANGLE_DEG;
}

void BacktrackManager::_computeCumulativeDistance() {
  if (_pointCount == 0) return;
  _cumDistanceM[0] = 0.0f;
  for (size_t i = 1; i < _pointCount; i++) {
    _cumDistanceM[i] = _cumDistanceM[i - 1];
    if (_points[i].segmentStart) continue;
    float d = geoDistanceKm(_points[i - 1].lat, _points[i - 1].lon,
                            _points[i].lat, _points[i].lon) * 1000.0f;
    if (isfinite(d) && d >= 0.0f) {
      _cumDistanceM[i] += d;
    }
  }
}

void BacktrackManager::_clearComputed() {
  _state.dataAvailable = false;
  _state.distanceToTargetM = NAN;
  _state.distanceToStartM = NAN;
  _state.remainingTrackDistanceM = NAN;
  _state.bearingDeg = NAN;
  _state.headingDeg = NAN;
  _state.relativeBearingDeg = NAN;
  _state.nearestRouteDistanceM = NAN;
  _state.nearestIndex = 0;
  _state.offTrack = false;
  _state.arrived = false;
}

void BacktrackManager::_setError(const char* msg) {
  if (!msg) msg = "";
  strncpy(_state.lastError, msg, sizeof(_state.lastError) - 1);
  _state.lastError[sizeof(_state.lastError) - 1] = '\0';
}

bool BacktrackManager::_validIndex(size_t index) const {
  return index < _pointCount;
}

size_t BacktrackManager::_chooseTargetIndex(size_t nearestIndex) {
  if (nearestIndex >= _pointCount) return 0;

  if (_state.targetMode == BacktrackTargetMode::ReturnNearestPoint) {
    return nearestIndex;
  }

  size_t lookahead = (_state.targetMode == BacktrackTargetMode::ReturnPreviousPoint)
                   ? 1
                   : BACKTRACK_TARGET_LOOKAHEAD_POINTS;
  size_t suggested = (nearestIndex > lookahead) ? nearestIndex - lookahead : 0;

  if (!_validIndex(_state.targetIndex)) return suggested;
  if (_state.distanceToTargetM <= BACKTRACK_ARRIVAL_RADIUS_M && _state.targetIndex > 0) {
    return _state.targetIndex - 1;
  }
  if (suggested < _state.targetIndex) {
    return suggested;
  }
  if (_state.targetIndex > nearestIndex) {
    return suggested;
  }
  return _state.targetIndex;
}

size_t BacktrackManager::_findNearestPoint(float lat, float lon,
                                           float& nearestPointDistanceM) const {
  size_t bestIndex = 0;
  nearestPointDistanceM = NAN;
  float best = 1.0e30f;

  for (size_t i = 0; i < _pointCount; i++) {
    float d = geoDistanceKm(lat, lon, _points[i].lat, _points[i].lon) * 1000.0f;
    if (isfinite(d) && d < best) {
      best = d;
      bestIndex = i;
    }
  }

  nearestPointDistanceM = best;
  return bestIndex;
}

float BacktrackManager::_distanceToRouteM(float lat, float lon) const {
  if (_pointCount == 0) return NAN;

  float best = 1.0e30f;
  for (size_t i = 0; i < _pointCount; i++) {
    float d = geoDistanceKm(lat, lon, _points[i].lat, _points[i].lon) * 1000.0f;
    if (isfinite(d) && d < best) best = d;

    if (i > 0 && !_points[i].segmentStart) {
      float sd = _segmentDistanceM(lat, lon, _points[i - 1], _points[i]);
      if (isfinite(sd) && sd < best) best = sd;
    }
  }
  return best;
}

float BacktrackManager::_remainingFromTargetM(size_t targetIndex,
                                              float distanceToTargetM) const {
  if (!_validIndex(targetIndex) || !isfinite(distanceToTargetM)) return NAN;
  return distanceToTargetM + _cumDistanceM[targetIndex];
}

bool BacktrackManager::_isValidCoordinate(float lat, float lon) {
  return isfinite(lat) && isfinite(lon) &&
         lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f;
}

float BacktrackManager::_normalize360(float deg) {
  if (!isfinite(deg)) return NAN;
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

float BacktrackManager::_normalizeRelative(float deg) {
  if (!isfinite(deg)) return NAN;
  while (deg <= -180.0f) deg += 360.0f;
  while (deg > 180.0f) deg -= 360.0f;
  return deg;
}

float BacktrackManager::_segmentDistanceM(float lat, float lon,
                                          const TrackPoint& a,
                                          const TrackPoint& b) {
  float cosLat = cosf(lat * GEO_DEG_TO_RAD);
  float ax = (a.lon - lon) * cosLat * 111320.0f;
  float ay = (a.lat - lat) * 111320.0f;
  float bx = (b.lon - lon) * cosLat * 111320.0f;
  float by = (b.lat - lat) * 111320.0f;
  float vx = bx - ax;
  float vy = by - ay;
  float len2 = vx * vx + vy * vy;
  if (len2 <= 0.01f) return sqrtf(ax * ax + ay * ay);

  float t = -(ax * vx + ay * vy) / len2;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  float px = ax + vx * t;
  float py = ay + vy * t;
  return sqrtf(px * px + py * py);
}
