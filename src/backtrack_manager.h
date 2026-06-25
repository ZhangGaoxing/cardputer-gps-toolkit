/**
 * backtrack_manager.h - frozen-track backtrack navigation state.
 */
#ifndef BACKTRACK_MANAGER_H
#define BACKTRACK_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "trip_tracker.h"

enum class BacktrackSource : uint8_t {
  CurrentTrip = 0,
  SavedGpx,
  SimplifiedTrack
};

enum class BacktrackTargetMode : uint8_t {
  ReturnToStart = 0,
  ReturnPreviousPoint,
  ReturnNearestPoint
};

struct BacktrackState {
  bool active = false;
  BacktrackSource source = BacktrackSource::CurrentTrip;
  BacktrackTargetMode targetMode = BacktrackTargetMode::ReturnToStart;
  size_t targetIndex = 0;

  bool dataAvailable = false;
  float distanceToTargetM = NAN;
  float distanceToStartM = NAN;
  float remainingTrackDistanceM = NAN;
  float bearingDeg = NAN;
  float headingDeg = NAN;
  float relativeBearingDeg = NAN;
  float nearestRouteDistanceM = NAN;
  size_t nearestIndex = 0;
  bool offTrack = false;
  bool arrived = false;
  char lastError[48] = "";
};

class BacktrackManager {
public:
  static BacktrackManager& instance();

  void begin();
  void update();
  bool startFromCurrentTrip(BacktrackTargetMode mode = BacktrackTargetMode::ReturnToStart);
  bool loadFromGpx(const char* path);
  void stop(const char* reason = nullptr);

  bool isActive() const { return _state.active; }
  const BacktrackState& state() const { return _state; }
  const char* lastError() const { return _state.lastError; }

  size_t pointCount() const { return _pointCount; }
  TrackPoint pointAt(size_t index) const;
  bool targetPoint(TrackPoint& out) const;
  bool startPoint(TrackPoint& out) const;

  static const char* sourceText(BacktrackSource source);
  static const char* modeText(BacktrackTargetMode mode);
  static const char* relativeText(float relDeg, bool headingValid);

private:
  BacktrackManager() = default;
  BacktrackManager(const BacktrackManager&) = delete;
  BacktrackManager& operator=(const BacktrackManager&) = delete;

  bool _buildSnapshotFromTrip();
  bool _copySimplified(float minDistanceM);
  bool _loadGpxSimplified(const char* path, float minDistanceM,
                          bool keepTurns, size_t& parsedPoints);
  bool _appendPoint(const TrackPoint& point);
  bool _appendGpxCandidate(const TrackPoint& point, TrackPoint& pending,
                           bool& hasPending, float minDistanceM,
                           bool keepTurns);
  bool _shouldKeepTurn(const TrackPoint& prev, const TrackPoint& cur,
                       const TrackPoint& next) const;
  void _computeCumulativeDistance();
  void _clearComputed();
  void _setError(const char* msg);
  bool _validIndex(size_t index) const;
  size_t _chooseTargetIndex(size_t nearestIndex);
  size_t _findNearestPoint(float lat, float lon, float& nearestPointDistanceM) const;
  float _distanceToRouteM(float lat, float lon) const;
  float _remainingFromTargetM(size_t targetIndex, float distanceToTargetM) const;

  static bool _isValidCoordinate(float lat, float lon);
  static float _normalize360(float deg);
  static float _normalizeRelative(float deg);
  static float _segmentDistanceM(float lat, float lon,
                                 const TrackPoint& a, const TrackPoint& b);

  BacktrackState _state;
  TrackPoint _points[BACKTRACK_MAX_POINTS];
  float _cumDistanceM[BACKTRACK_MAX_POINTS];
  size_t _pointCount = 0;
  unsigned long _lastRecalcMs = 0;
};

#endif // BACKTRACK_MANAGER_H
