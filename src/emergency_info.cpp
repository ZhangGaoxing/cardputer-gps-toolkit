#include "emergency_info.h"

#include "backtrack_manager.h"
#include "battery_manager.h"
#include "geo_format.h"
#include "geo_math.h"
#include "gps_manager.h"
#include "navigation_manager.h"
#include "rtc_manager.h"
#include "trip_tracker.h"
#include "ui_helpers.h"
#include "waypoint_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

const char* kEmergencyDeviceName = "CARDPUTER GPS";

const char* gpsStatusText(const GPSManager& gps) {
  if (!gps.hasRecentGnssData()) return "No Data";
  if (gps.hasReliableFix()) return "Reliable Fix";
  if (gps.hasFreshFix()) return "Fresh Fix";
  return "No Fix";
}

void formatDistanceShort(char* out, size_t outSize, float meters) {
  if (!out || outSize == 0) return;
  if (!isfinite(meters)) {
    snprintf(out, outSize, "--");
  } else if (meters < 1000.0f) {
    snprintf(out, outSize, "%.0fm", meters);
  } else if (meters < 10000.0f) {
    snprintf(out, outSize, "%.2fkm", meters / 1000.0f);
  } else {
    snprintf(out, outSize, "%.1fkm", meters / 1000.0f);
  }
}

void formatTripDistance(char* out, size_t outSize, float km) {
  if (!out || outSize == 0) return;
  if (!isfinite(km) || km < 0.0f) {
    snprintf(out, outSize, "--");
  } else if (km < 10.0f) {
    snprintf(out, outSize, "%.2fkm", km);
  } else {
    snprintf(out, outSize, "%.1fkm", km);
  }
}

const char* fixModeText(int fixMode) {
  if (fixMode >= 3) return "3D";
  if (fixMode == 2) return "2D";
  return "NF";
}

void appendToken(char* out, size_t outSize, const char* token) {
  if (!out || outSize == 0 || !token || token[0] == '\0') return;
  size_t used = strlen(out);
  if (used >= outSize - 1) return;
  if (used != 0) {
    int written = snprintf(out + used, outSize - used, " | %s", token);
    if (written < 0) return;
  } else {
    snprintf(out + used, outSize - used, "%s", token);
  }
}

void copyWaypointName(char* out, size_t outSize, const char* value) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!value) return;
  strncpy(out, value, outSize - 1);
  out[outSize - 1] = '\0';
}

void fillChosenPosition(EmergencySnapshot& out, const ReliableFixSnapshot& fix,
                        uint32_t ageMs, EmergencyPositionSource source) {
  out.positionAvailable = true;
  out.positionSource = source;
  out.lat = fix.lat;
  out.lon = fix.lon;
  out.altitudeValid = fix.altValid;
  out.altM = fix.altM;
  out.satellites = fix.satellitesUsed;
  out.hdop = fix.hdop;
  out.fixMode = fix.fixMode;
  out.fixQuality = fix.fixQuality;
  out.positionWarnAge = ageMs >= SOS_LAST_FIX_WARN_AGE_MS;
  out.positionExpired = ageMs >= SOS_LAST_FIX_MAX_AGE_MS;
  snprintf(out.positionLabel, sizeof(out.positionLabel), "%s",
           source == EMERGENCY_POS_CURRENT ? "Current" : "Last Known");
  if (source == EMERGENCY_POS_CURRENT) {
    snprintf(out.credibilityText, sizeof(out.credibilityText), "Reliable current GNSS fix");
    snprintf(out.lastFixAgeText, sizeof(out.lastFixAgeText), "Last fix: now");
  } else {
    char ageBuf[16];
    EmergencyInfo::formatAgeShort(ageMs, ageBuf, sizeof(ageBuf));
    snprintf(out.lastFixAgeText, sizeof(out.lastFixAgeText), "Last fix: %s ago", ageBuf);
    if (out.positionExpired) {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "Last reliable fix expired");
    } else if (out.positionWarnAge) {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "Last reliable fix aging");
    } else {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "Last reliable position");
    }
  }
}

} // namespace

void EmergencyInfo::buildSnapshot(EmergencySnapshot& out) {
  out = EmergencySnapshot();
  memset(out.deviceName, 0, sizeof(out.deviceName));
  memset(out.gpsStatus, 0, sizeof(out.gpsStatus));
  memset(out.positionLabel, 0, sizeof(out.positionLabel));
  memset(out.credibilityText, 0, sizeof(out.credibilityText));
  memset(out.timeText, 0, sizeof(out.timeText));
  memset(out.payloadTimeText, 0, sizeof(out.payloadTimeText));
  memset(out.lastFixAgeText, 0, sizeof(out.lastFixAgeText));
  memset(out.tipText, 0, sizeof(out.tipText));
  memset(out.nearestWaypoint.name, 0, sizeof(out.nearestWaypoint.name));
  memset(out.navigation.targetName, 0, sizeof(out.navigation.targetName));
  memset(out.backtrack.status, 0, sizeof(out.backtrack.status));

  snprintf(out.deviceName, sizeof(out.deviceName), "%s", kEmergencyDeviceName);

  GPSManager& gps = GPSManager::instance();
  RTCManager& rtc = RTCManager::instance();
  BatteryManager& battery = BatteryManager::instance();
  TripTracker& trip = TripTracker::instance();
  WaypointManager& waypoints = WaypointManager::instance();
  NavigationManager& nav = NavigationManager::instance();
  BacktrackManager& backtrack = BacktrackManager::instance();

  out.gpsHasData = gps.hasRecentGnssData();
  out.gpsHasFreshFix = gps.hasFreshFix();
  out.gpsHasReliableFix = gps.hasReliableFix();
  snprintf(out.gpsStatus, sizeof(out.gpsStatus), "%s", gpsStatusText(gps));

  if (out.gpsHasReliableFix) {
    ReliableFixSnapshot currentFix;
    currentFix.valid = true;
    currentFix.lat = gps.latitude();
    currentFix.lon = gps.longitude();
    currentFix.altM = gps.altitude();
    currentFix.altValid = gps.altitudeValid();
    currentFix.hdop = gps.hdop();
    currentFix.satellitesUsed = gps.satellitesUsed();
    currentFix.fixQuality = gps.fixQuality();
    currentFix.fixMode = gps.fixMode();
    fillChosenPosition(out, currentFix, gps.fixAgeMs(), EMERGENCY_POS_CURRENT);
  } else if (gps.hasLastReliableFix()) {
    fillChosenPosition(out, gps.lastReliableFix(), gps.lastReliableFixAgeMs(),
                       EMERGENCY_POS_LAST_KNOWN);
  } else {
    snprintf(out.positionLabel, sizeof(out.positionLabel), "Unavailable");
    snprintf(out.lastFixAgeText, sizeof(out.lastFixAgeText), "Last fix: none");
    if (out.gpsHasFreshFix) {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "Fresh fix not reliable yet");
    } else if (out.gpsHasData) {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "Waiting for reliable fix");
    } else {
      snprintf(out.credibilityText, sizeof(out.credibilityText), "No GNSS data");
    }
  }

  if (rtc.isSynced() && rtc.dateValid()) {
    out.usingLocalTime = true;
    snprintf(out.timeText, sizeof(out.timeText),
             "Local %04d-%02d-%02d %02d:%02d:%02d",
             rtc.localYear(), rtc.localMonth(), rtc.localDay(),
             rtc.localHour(), rtc.localMinute(), rtc.localSecond());
    snprintf(out.payloadTimeText, sizeof(out.payloadTimeText),
             "%04d-%02d-%02dT%02d:%02d:%02dL",
             rtc.localYear(), rtc.localMonth(), rtc.localDay(),
             rtc.localHour(), rtc.localMinute(), rtc.localSecond());
  } else if (gps.dateValid() && gps.timeValid()) {
    snprintf(out.timeText, sizeof(out.timeText),
             "UTC %04d-%02d-%02d %02d:%02d:%02d",
             gps.utcYear(), gps.utcMonth(), gps.utcDay(),
             gps.utcHour(), gps.utcMinute(), gps.utcSecond());
    snprintf(out.payloadTimeText, sizeof(out.payloadTimeText),
             "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps.utcYear(), gps.utcMonth(), gps.utcDay(),
             gps.utcHour(), gps.utcMinute(), gps.utcSecond());
  } else if (gps.hasLastReliableFix()) {
    const ReliableFixSnapshot& lastFix = gps.lastReliableFix();
    if (lastFix.dateValid && lastFix.timeValid) {
      snprintf(out.timeText, sizeof(out.timeText),
               "UTC %04d-%02d-%02d %02d:%02d:%02d",
               lastFix.year, lastFix.month, lastFix.day,
               lastFix.hour, lastFix.minute, lastFix.second);
      snprintf(out.payloadTimeText, sizeof(out.payloadTimeText),
               "%04d-%02d-%02dT%02d:%02d:%02dZ",
               lastFix.year, lastFix.month, lastFix.day,
               lastFix.hour, lastFix.minute, lastFix.second);
    }
  }
  if (out.timeText[0] == '\0') {
    snprintf(out.timeText, sizeof(out.timeText), "Time unavailable");
  }

  out.batteryPct = battery.percentage();
  out.batteryCharging = battery.isCharging();
  out.tripStarted = trip.stats().hasPrev && trip.pointCount() > 0;
  out.tripDistanceKm = trip.totalDistanceKm();

  if (out.tripStarted && out.positionAvailable) {
    TrackPoint start = trip.pointAt(0);
    if (geoIsValidCoordinate(start.lat, start.lon) && geoIsValidCoordinate(out.lat, out.lon)) {
      out.distanceToStartM = geoDistanceKm(out.lat, out.lon, start.lat, start.lon) * 1000.0f;
      out.distanceToStartAvailable = isfinite(out.distanceToStartM);
    }
  }

  if (out.positionAvailable && waypoints.count() > 0) {
    int nearestIndex = waypoints.findNearest(out.lat, out.lon);
    const Waypoint* nearest = waypoints.getByIndex(nearestIndex);
    if (nearest) {
      float distM = geoDistanceKm(out.lat, out.lon, nearest->lat, nearest->lon) * 1000.0f;
      if (isfinite(distM) && distM <= SOS_NEAREST_WAYPOINT_MAX_DISTANCE_M) {
        out.nearestWaypoint.available = true;
        copyWaypointName(out.nearestWaypoint.name, sizeof(out.nearestWaypoint.name), nearest->name);
        out.nearestWaypoint.distanceM = distM;
        out.nearestWaypoint.bearingDeg = geoBearingDeg(out.lat, out.lon, nearest->lat, nearest->lon);
      }
    }
  }

  out.navigation.active = nav.isActive();
  if (out.navigation.active) {
    const NavigationState& navState = nav.state();
    copyWaypointName(out.navigation.targetName, sizeof(out.navigation.targetName), navState.targetName);
    if (navState.dataAvailable && isfinite(navState.distanceM)) {
      out.navigation.distanceAvailable = true;
      out.navigation.distanceM = navState.distanceM;
      out.navigation.bearingDeg = navState.bearingDeg;
    } else if (out.positionAvailable && NavigationManager::isValidCoordinate(navState.targetLat, navState.targetLon)) {
      out.navigation.distanceM = geoDistanceKm(out.lat, out.lon, navState.targetLat, navState.targetLon) * 1000.0f;
      out.navigation.bearingDeg = geoBearingDeg(out.lat, out.lon, navState.targetLat, navState.targetLon);
      out.navigation.distanceAvailable = isfinite(out.navigation.distanceM);
    }
  }

  out.backtrack.active = backtrack.isActive();
  if (out.backtrack.active) {
    const BacktrackState& btState = backtrack.state();
    if (btState.lastError[0] != '\0') {
      snprintf(out.backtrack.status, sizeof(out.backtrack.status), "%s", btState.lastError);
    } else if (btState.offTrack) {
      snprintf(out.backtrack.status, sizeof(out.backtrack.status), "Off Track");
    } else {
      snprintf(out.backtrack.status, sizeof(out.backtrack.status), "Active");
    }

    TrackPoint startPoint;
    if (out.positionAvailable && backtrack.startPoint(startPoint) &&
        geoIsValidCoordinate(startPoint.lat, startPoint.lon)) {
      out.backtrack.distanceToStartM =
        geoDistanceKm(out.lat, out.lon, startPoint.lat, startPoint.lon) * 1000.0f;
      out.backtrack.distanceToStartAvailable = isfinite(out.backtrack.distanceToStartM);
    }

    if (btState.dataAvailable && isfinite(btState.remainingTrackDistanceM)) {
      out.backtrack.remainingM = btState.remainingTrackDistanceM;
      out.backtrack.remainingAvailable = true;
    }
  } else {
    snprintf(out.backtrack.status, sizeof(out.backtrack.status), "Inactive");
  }

  if (!out.gpsHasData) {
    snprintf(out.tipText, sizeof(out.tipText), "Move outdoors and keep the antenna clear.");
  } else if (out.positionExpired) {
    snprintf(out.tipText, sizeof(out.tipText), "Last known fix is stale. State its age clearly.");
  } else if (out.gpsHasFreshFix && !out.gpsHasReliableFix) {
    snprintf(out.tipText, sizeof(out.tipText), "Current fix is not reliable. Use last known.");
  } else if (out.batteryPct <= 15) {
    snprintf(out.tipText, sizeof(out.tipText), "Low battery. Share payload and reduce screen time.");
  } else {
    snprintf(out.tipText, sizeof(out.tipText), "Share coords, battery, trip and nearest waypoint.");
  }
}

size_t EmergencyInfo::buildPayload(const EmergencySnapshot& snapshot, char* out, size_t outSize) {
  if (!out || outSize == 0) return 0;
  out[0] = '\0';

  char token[96];
  appendToken(out, outSize, snapshot.deviceName);
  appendToken(out, outSize, "SOS");

  if (snapshot.positionAvailable) {
    char latBuf[20];
    char lonBuf[20];
    geoFormatCoordinateDecimal(latBuf, sizeof(latBuf), snapshot.lat, true);
    geoFormatCoordinateDecimal(lonBuf, sizeof(lonBuf), snapshot.lon, false);
    snprintf(token, sizeof(token), "POS %s LAT %s LON %s",
             snapshot.positionSource == EMERGENCY_POS_CURRENT ? "CUR" : "LAST",
             latBuf, lonBuf);
    appendToken(out, outSize, token);

    if (snapshot.positionSource == EMERGENCY_POS_LAST_KNOWN) {
      appendToken(out, outSize, snapshot.lastFixAgeText);
      if (snapshot.positionExpired) appendToken(out, outSize, "STALE");
    }

    if (snapshot.altitudeValid && isfinite(snapshot.altM)) {
      snprintf(token, sizeof(token), "ELE %.0fm", snapshot.altM);
      appendToken(out, outSize, token);
    }
  } else {
    appendToken(out, outSize, "POS NONE");
  }

  // TIME is placed early so it survives payload truncation
  if (snapshot.payloadTimeText[0] != '\0') {
    snprintf(token, sizeof(token), "TIME %s", snapshot.payloadTimeText);
    appendToken(out, outSize, token);
  }

  snprintf(token, sizeof(token), "FIX %s HDOP %.1f SAT %d %s",
           snapshot.gpsStatus,
           snapshot.hdop,
           snapshot.satellites,
           fixModeText(snapshot.fixMode));
  appendToken(out, outSize, token);

  snprintf(token, sizeof(token), "BAT %d%%%s",
           snapshot.batteryPct,
           snapshot.batteryCharging ? " CHG" : "");
  appendToken(out, outSize, token);

  if (snapshot.nearestWaypoint.available) {
    char distBuf[16];
    formatDistanceShort(distBuf, sizeof(distBuf), snapshot.nearestWaypoint.distanceM);
    snprintf(token, sizeof(token), "NEAR %s %s %s",
             snapshot.nearestWaypoint.name,
             distBuf,
             cardinalFromHeading(snapshot.nearestWaypoint.bearingDeg));
    appendToken(out, outSize, token);
  }

  if (snapshot.tripStarted) {
    char tripBuf[16];
    formatTripDistance(tripBuf, sizeof(tripBuf), snapshot.tripDistanceKm);
    snprintf(token, sizeof(token), "TRIP %s", tripBuf);
    appendToken(out, outSize, token);
  }

  if (snapshot.distanceToStartAvailable) {
    char distBuf[16];
    formatDistanceShort(distBuf, sizeof(distBuf), snapshot.distanceToStartM);
    snprintf(token, sizeof(token), "START %s", distBuf);
    appendToken(out, outSize, token);
  }

  if (snapshot.backtrack.active) {
    if (snapshot.backtrack.remainingAvailable) {
      char distBuf[16];
      formatDistanceShort(distBuf, sizeof(distBuf), snapshot.backtrack.remainingM);
      snprintf(token, sizeof(token), "BT %s %s", snapshot.backtrack.status, distBuf);
    } else {
      snprintf(token, sizeof(token), "BT %s", snapshot.backtrack.status);
    }
    appendToken(out, outSize, token);
  }

  if (snapshot.navigation.active) {
    if (snapshot.navigation.distanceAvailable) {
      char distBuf[16];
      formatDistanceShort(distBuf, sizeof(distBuf), snapshot.navigation.distanceM);
      snprintf(token, sizeof(token), "NAV %s %s", snapshot.navigation.targetName, distBuf);
    } else {
      snprintf(token, sizeof(token), "NAV %s", snapshot.navigation.targetName);
    }
    appendToken(out, outSize, token);
  }

  // appendToken already guarantees strlen(out) < outSize; this is a safety net only
  out[outSize - 1] = '\0';
  return strlen(out);
}

void EmergencyInfo::formatAgeShort(uint32_t ageMs, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  if (ageMs == UINT32_MAX) {
    snprintf(out, outSize, "--");
    return;
  }

  uint32_t totalSec = ageMs / 1000UL;
  if (totalSec < 60UL) {
    snprintf(out, outSize, "%lus", (unsigned long)totalSec);
    return;
  }

  uint32_t totalMin = totalSec / 60UL;
  if (totalMin < 60UL) {
    snprintf(out, outSize, "%lum", (unsigned long)totalMin);
    return;
  }

  uint32_t hours = totalMin / 60UL;
  uint32_t minutes = totalMin % 60UL;
  snprintf(out, outSize, "%luh%02lum", (unsigned long)hours, (unsigned long)minutes);
}

const char* EmergencyInfo::positionSourceText(EmergencyPositionSource source) {
  switch (source) {
    case EMERGENCY_POS_CURRENT: return "Current";
    case EMERGENCY_POS_LAST_KNOWN: return "Last Known";
    default: return "Unavailable";
  }
}