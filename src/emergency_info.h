#ifndef EMERGENCY_INFO_H
#define EMERGENCY_INFO_H

#include <Arduino.h>
#include "config.h"

enum EmergencyPositionSource : uint8_t {
  EMERGENCY_POS_NONE = 0,
  EMERGENCY_POS_CURRENT,
  EMERGENCY_POS_LAST_KNOWN
};

struct EmergencyWaypointInfo {
  bool available = false;
  char name[WAYPOINT_NAME_MAX_LEN];
  float distanceM = NAN;
  float bearingDeg = NAN;
};

struct EmergencyNavigationInfo {
  bool active = false;
  bool distanceAvailable = false;
  char targetName[WAYPOINT_NAME_MAX_LEN];
  float distanceM = NAN;
  float bearingDeg = NAN;
};

struct EmergencyBacktrackInfo {
  bool active = false;
  bool distanceToStartAvailable = false;
  bool remainingAvailable = false;
  char status[48];
  float distanceToStartM = NAN;
  float remainingM = NAN;
};

struct EmergencySnapshot {
  char deviceName[24];
  char gpsStatus[16];
  char positionLabel[20];
  char credibilityText[32];
  char timeText[28];
  char payloadTimeText[24];
  char lastFixAgeText[24];
  char tipText[48];
  bool gpsHasData = false;
  bool gpsHasFreshFix = false;
  bool gpsHasReliableFix = false;
  bool positionAvailable = false;
  bool positionWarnAge = false;
  bool positionExpired = false;
  bool usingLocalTime = false;
  EmergencyPositionSource positionSource = EMERGENCY_POS_NONE;
  float lat = NAN;
  float lon = NAN;
  bool altitudeValid = false;
  float altM = NAN;
  int satellites = 0;
  float hdop = 99.9f;
  int fixMode = 1;
  int fixQuality = 0;
  int batteryPct = 0;
  bool batteryCharging = false;
  bool tripStarted = false;
  float tripDistanceKm = 0.0f;
  bool distanceToStartAvailable = false;
  float distanceToStartM = NAN;
  EmergencyWaypointInfo nearestWaypoint;
  EmergencyNavigationInfo navigation;
  EmergencyBacktrackInfo backtrack;
};

class EmergencyInfo {
public:
  static void buildSnapshot(EmergencySnapshot& out);
  static size_t buildPayload(const EmergencySnapshot& snapshot, char* out, size_t outSize);
  static void formatAgeShort(uint32_t ageMs, char* out, size_t outSize);
  static const char* positionSourceText(EmergencyPositionSource source);
};

#endif // EMERGENCY_INFO_H