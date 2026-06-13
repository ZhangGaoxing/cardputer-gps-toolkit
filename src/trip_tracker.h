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
  bool segmentStart;
};

struct TripFixQuality {
  bool reliableFix = false;
  float hdop = 99.9f;
  int satellitesUsed = 0;
  uint32_t locationAgeMs = UINT32_MAX;
};

struct TripStats {
  float totalDistKm = 0;
  float maxSpeedKmph = 0;
  float maxAltM = -99999, minAltM = 99999;
  float totalAscentM = 0, totalDescentM = 0;
  float prevLat=0, prevLon=0, prevAlt=0;
  bool hasPrev = false;
  uint32_t startMillis = 0;
  uint32_t prevMillis = 0;
  uint32_t movingMillis = 0;
  uint32_t lastMovingCheck = 0;
  uint32_t rejectedPoints = 0;
  float currentSpeedKmph = 0;
};

class TripTracker {
public:
  static TripTracker& instance();
  void begin();
  bool update(float lat, float lon, float alt, float speed, const TripFixQuality& quality);
  bool update(float lat, float lon, float alt, float speed, bool locValid);
  size_t pointCount() const { return _count; }
  TrackPoint pointAt(size_t index) const;
  const TripStats& stats() const { return _stats; }
  float totalDistanceKm() const;
  bool lastUpdateRejected() const { return _lastUpdateRejected; }
  bool lastAcceptedStartedSegment() const { return _lastAcceptedStartedSegment; }
private:
  void _recordPoint(float lat, float lon, float alt, float speed, uint32_t now,
                    bool segmentStart = false);
  void _rejectPoint(uint32_t now);
  bool _isFiniteCoordinate(float lat, float lon, float alt, float speed) const;
  TrackPoint _buf[TRACK_MAX];
  size_t _head=0, _count=0;
  TripStats _stats;
  uint32_t _lastRecord=0;
  uint32_t _lastReject=0;
  uint32_t _fixLostMs=0;
  bool _hadFixGap=false;
  bool _lastUpdateRejected = false;
  bool _lastAcceptedStartedSegment = false;
};

#endif
