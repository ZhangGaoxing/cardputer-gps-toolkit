/**
 * trip_tracker.h — 轨迹记录与行程统计
 * 【Phase 2将完整移植GPSInfo的TrackPoint/TripStats代码】
 */
#ifndef TRIP_TRACKER_H
#define TRIP_TRACKER_H

#include <Arduino.h>
#include "config.h"

struct TrackPoint {
  float lat, lon;
  float altM;
  float speedKmph;
  uint32_t timestamp;
};

struct TripStats {
  float totalDistKm = 0;
  float maxSpeedKmph = 0;
  float maxAltM = -99999, minAltM = 99999;
  float totalAscentM = 0, totalDescentM = 0;
  float prevLat=0, prevLon=0, prevAlt=0;
  bool hasPrev = false;
  uint32_t startMillis = 0;
  uint32_t movingMillis = 0;
  uint32_t lastMovingCheck = 0;
};

class TripTracker {
public:
  static TripTracker& instance();
  void begin();
  void update(float lat, float lon, float alt, float speed, bool locValid);
  const TrackPoint* points() const { return _buf; }
  int pointCount() const { return _count; }
  const TripStats& stats() const { return _stats; }
  float totalDistanceKm() const;
private:
  TrackPoint _buf[TRACK_MAX];
  int _head=0, _count=0;
  TripStats _stats;
  uint32_t _lastRecord=0;
};

#endif
