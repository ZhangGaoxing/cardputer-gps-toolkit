/**
 * navigation_manager.cpp - runtime Go-to waypoint navigation state.
 */
#include "navigation_manager.h"
#include "gps_manager.h"
#include "geo_math.h"

#include <math.h>
#include <string.h>

NavigationManager& NavigationManager::instance() {
  static NavigationManager nav;
  return nav;
}

void NavigationManager::begin() {
  memset(&_state, 0, sizeof(_state));
  _clearComputed();
#if NAV_PERSIST_TARGET_ENABLED
  _loadTarget();
#endif
}

bool NavigationManager::startGoto(uint16_t waypointId) {
  WaypointManager& wm = WaypointManager::instance();
  const Waypoint* wp = wm.getById(waypointId);
  if (!wp) {
    _setError("Waypoint not found");
    return false;
  }
  if (!isValidCoordinate(wp->lat, wp->lon)) {
    _setError("Invalid target");
    return false;
  }

  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  _state.active = true;
  _state.targetId = wp->id;
  strncpy(_state.targetName, wp->name, sizeof(_state.targetName) - 1);
  _state.targetName[sizeof(_state.targetName) - 1] = '\0';
  _state.targetLat = wp->lat;
  _state.targetLon = wp->lon;
  _state.targetEle = wp->ele;
  _setError("");
  _saveTarget();
  update();
  return true;
}

void NavigationManager::stop(const char* reason) {
  memset(&_state, 0, sizeof(_state));
  _clearComputed();
  if (reason && reason[0] != '\0') {
    _setError(reason);
  }
  _clearSavedTarget();
}

void NavigationManager::update() {
  if (!_state.active) return;

  WaypointManager& wm = WaypointManager::instance();
  const Waypoint* wp = wm.getById(_state.targetId);
  if (!wp) {
    stop("Target deleted");
    return;
  }
  if (!isValidCoordinate(wp->lat, wp->lon)) {
    stop("Invalid target");
    return;
  }

  strncpy(_state.targetName, wp->name, sizeof(_state.targetName) - 1);
  _state.targetName[sizeof(_state.targetName) - 1] = '\0';
  _state.targetLat = wp->lat;
  _state.targetLon = wp->lon;
  _state.targetEle = wp->ele;

  GPSManager& gps = GPSManager::instance();
  if (!gps.hasReliableFix()) {
    _clearComputed();
    _state.active = true;
    _state.targetId = wp->id;
    strncpy(_state.targetName, wp->name, sizeof(_state.targetName) - 1);
    _state.targetName[sizeof(_state.targetName) - 1] = '\0';
    _state.targetLat = wp->lat;
    _state.targetLon = wp->lon;
    _state.targetEle = wp->ele;
    _setError("No reliable fix");
    return;
  }

  float lat = gps.latitude();
  float lon = gps.longitude();
  if (!isValidCoordinate(lat, lon)) {
    _clearComputed();
    _state.active = true;
    _setError("Invalid GPS");
    return;
  }

  _state.distanceM = geoDistanceKm(lat, lon, wp->lat, wp->lon) * 1000.0f;
  _state.bearingDeg = geoBearingDeg(lat, lon, wp->lat, wp->lon);
  _state.dataAvailable = isfinite(_state.distanceM) && isfinite(_state.bearingDeg);

  float speedMps = gps.speedMps();
  bool headingValid = gps.courseValid() &&
                      gps.speedValid() &&
                      speedMps >= NAV_MIN_SPEED_FOR_ETA_MPS;
  if (headingValid) {
    _state.headingDeg = normalize360(gps.courseDeg());
    _state.relativeBearingDeg = normalizeRelative(_state.bearingDeg - _state.headingDeg);
  } else {
    _state.headingDeg = NAN;
    _state.relativeBearingDeg = NAN;
  }

  _state.arrived = _state.dataAvailable &&
                   gps.hdop() < GPS_RELIABLE_HDOP_MAX &&
                   _state.distanceM <= NAV_ARRIVAL_RADIUS_M;

  if (_state.dataAvailable && gps.speedValid() && speedMps >= NAV_MIN_SPEED_FOR_ETA_MPS) {
    float rawEta = _state.distanceM / speedMps;
    if (isfinite(rawEta) && rawEta >= 0.0f) {
      if (!isfinite(_smoothedEtaSec)) {
        _smoothedEtaSec = rawEta;
      } else {
        _smoothedEtaSec = NAV_ETA_SMOOTHING_ALPHA * rawEta +
                          (1.0f - NAV_ETA_SMOOTHING_ALPHA) * _smoothedEtaSec;
      }
      _state.etaAvailable = true;
      _state.etaSeconds = (uint32_t)(_smoothedEtaSec + 0.5f);
    } else {
      _state.etaAvailable = false;
    }
  } else {
    _state.etaAvailable = false;
    _smoothedEtaSec = NAN;
  }

  _setError("");
}

bool NavigationManager::isValidCoordinate(float lat, float lon) {
  return isfinite(lat) && isfinite(lon) &&
         lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f;
}

float NavigationManager::normalize360(float deg) {
  if (!isfinite(deg)) return NAN;
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

float NavigationManager::normalizeRelative(float deg) {
  if (!isfinite(deg)) return NAN;
  while (deg <= -180.0f) deg += 360.0f;
  while (deg > 180.0f) deg -= 360.0f;
  return deg;
}

const char* NavigationManager::relativeText(float relDeg, bool headingValid) {
  if (!headingValid || !isfinite(relDeg)) return "--";
  float a = fabs(relDeg);
  if (a <= 15.0f) return "Ahead";
  if (a >= 150.0f) return "Behind";
  if (a >= NAV_OFF_COURSE_ANGLE_DEG) return relDeg > 0.0f ? "Turn R" : "Turn L";
  return relDeg > 0.0f ? "Right" : "Left";
}

void NavigationManager::_clearComputed() {
  _state.dataAvailable = false;
  _state.distanceM = NAN;
  _state.bearingDeg = NAN;
  _state.headingDeg = NAN;
  _state.relativeBearingDeg = NAN;
  _state.etaAvailable = false;
  _state.etaSeconds = 0;
  _state.arrived = false;
  _smoothedEtaSec = NAN;
}

void NavigationManager::_setError(const char* msg) {
  if (!msg) msg = "";
  strncpy(_state.lastError, msg, sizeof(_state.lastError) - 1);
  _state.lastError[sizeof(_state.lastError) - 1] = '\0';
}

bool NavigationManager::_loadTarget() {
  return false;
}

void NavigationManager::_saveTarget() {
}

void NavigationManager::_clearSavedTarget() {
}
