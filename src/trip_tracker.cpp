#include "trip_tracker.h"
#include "ui_helpers.h"

TripTracker& TripTracker::instance() { static TripTracker tt; return tt; }

void TripTracker::begin() {
  memset(_buf,0,sizeof(_buf));
  _head=_count=0;
  _stats=TripStats();
  _lastRecord=0;
  _lastReject=0;
  _fixLostMs=0;
  _hadFixGap=false;
  _lastUpdateRejected=false;
  _lastAcceptedStartedSegment=false;
}

bool TripTracker::update(float lat,float lon,float alt,float speed,bool locValid) {
  TripFixQuality quality;
  quality.reliableFix = locValid;
  quality.hdop = locValid ? 0.0f : 99.9f;
  quality.satellitesUsed = locValid ? TRACK_MIN_SATELLITES : 0;
  quality.locationAgeMs = locValid ? 0 : UINT32_MAX;
  return update(lat, lon, alt, speed, quality);
}

bool TripTracker::update(float lat,float lon,float alt,float speed,const TripFixQuality& quality) {
  uint32_t now=millis();
  _lastUpdateRejected = false;
  _lastAcceptedStartedSegment = false;

  if(!quality.reliableFix ||
     quality.hdop > TRACK_MAX_HDOP ||
     quality.satellitesUsed < TRACK_MIN_SATELLITES ||
     quality.locationAgeMs > TRACK_MAX_LOCATION_AGE_MS ||
     !_isFiniteCoordinate(lat, lon, alt, speed)) {
    _lastUpdateRejected = true;
    if(!_hadFixGap) {
      _fixLostMs = now;
      _hadFixGap = true;
    }
    _rejectPoint(now);
    return false;
  }

  if(!_stats.hasPrev) {
    _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=alt;
    _stats.hasPrev=true; _stats.startMillis=now;
    _stats.prevMillis=now;
    _stats.lastMovingCheck=now;
    _stats.minAltM=_stats.maxAltM=alt;
    _stats.currentSpeedKmph=speed;
    _hadFixGap=false;
    _fixLostMs=0;
    _recordPoint(lat, lon, alt, speed, now, true);
    return true;
  }

  bool startNewSegment = _hadFixGap &&
                         _fixLostMs != 0 &&
                         now - _fixLostMs >= TRACK_FIX_GAP_BREAK_MS;
  _hadFixGap=false;
  _fixLostMs=0;

  if(startNewSegment) {
    _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=alt;
    _stats.prevMillis=now;
    _stats.lastMovingCheck=now;
    _stats.currentSpeedKmph=speed;
    _recordPoint(lat, lon, alt, speed, now, true);
    return true;
  }

  uint32_t dtMs = now - _stats.lastMovingCheck;
  if(dtMs < TRACK_MIN_SAMPLE_INTERVAL_MS) return false;

  uint32_t segmentMs = now - _stats.prevMillis;
  float dist=haversineKm(_stats.prevLat,_stats.prevLon,lat,lon);
  float distM = dist * 1000.0f;
  float impliedSpeedKmph = (segmentMs > 0) ? (dist / (segmentMs / 3600000.0f)) : 0.0f;

  if(speed > TRACK_MAX_REASONABLE_SPEED_KMPH ||
     impliedSpeedKmph > TRACK_MAX_REASONABLE_SPEED_KMPH) {
    _lastUpdateRejected = true;
    _rejectPoint(now);
    return false;
  }

  if(speed <= TRACK_STATIONARY_SPEED_KMPH && distM <= TRACK_STATIONARY_DRIFT_M) {
    _stats.currentSpeedKmph = 0;
    _stats.lastMovingCheck = now;
    _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=alt;
    _stats.prevMillis=now;
    if(now-_lastRecord>=TRACK_RECORD_INTERVAL) {
      _recordPoint(lat, lon, alt, 0, now);
      return true;
    }
    return false;
  }

  float dAlt=alt-_stats.prevAlt;
  bool altAccepted = fabs(dAlt) <= TRACK_MAX_ALT_JUMP_M;
  float acceptedAlt = altAccepted ? alt : _stats.prevAlt;

  if(altAccepted) {
    if(acceptedAlt>_stats.maxAltM) _stats.maxAltM=acceptedAlt;
    if(acceptedAlt<_stats.minAltM) _stats.minAltM=acceptedAlt;
    if(dAlt>0.5) _stats.totalAscentM+=dAlt;
    else if(dAlt<-0.5) _stats.totalDescentM-=dAlt;
  } else {
    _lastUpdateRejected = true;
    _rejectPoint(now);
    return false;
  }

  if(distM < TRACK_MIN_VALID_MOVE_M) {
    _stats.currentSpeedKmph = 0;
    _stats.lastMovingCheck=now;
    _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=acceptedAlt;
    _stats.prevMillis=now;
    if(now-_lastRecord>=TRACK_RECORD_INTERVAL) {
      _recordPoint(lat, lon, acceptedAlt, 0, now);
      return true;
    }
    return false;
  }

  _stats.totalDistKm+=dist;
  if(speed>_stats.maxSpeedKmph) _stats.maxSpeedKmph=speed;

  if(speed>TRACK_STATIONARY_SPEED_KMPH) {
    _stats.movingMillis+=dtMs;
  }
  _stats.lastMovingCheck=now;
  _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=acceptedAlt;
  _stats.prevMillis=now;
  _stats.currentSpeedKmph = speed;

  if(now-_lastRecord>=TRACK_RECORD_INTERVAL) {
    _recordPoint(lat, lon, acceptedAlt, _stats.currentSpeedKmph, now);
    return true;
  }
  return false;
}

float TripTracker::totalDistanceKm() const { return _stats.totalDistKm; }

TrackPoint TripTracker::pointAt(size_t index) const {
  if(index >= _count) return TrackPoint{0, 0, 0, 0, 0};
  size_t oldest = (_head + TRACK_MAX - _count) % TRACK_MAX;
  return _buf[(oldest + index) % TRACK_MAX];
}

void TripTracker::_recordPoint(float lat, float lon, float alt, float speed,
                               uint32_t now, bool segmentStart) {
  _lastRecord=now;
  _lastAcceptedStartedSegment=segmentStart;
  TrackPoint& p=_buf[_head];
  p.lat=lat; p.lon=lon; p.altM=alt; p.speedKmph=speed; p.timestamp=now;
  p.segmentStart=segmentStart;
  _head=(_head+1)%TRACK_MAX;
  if(_count<TRACK_MAX) _count++;
}

void TripTracker::_rejectPoint(uint32_t now) {
  if(now - _lastReject < TRACK_MIN_SAMPLE_INTERVAL_MS) return;
  _lastReject = now;
  _stats.rejectedPoints++;
}

bool TripTracker::_isFiniteCoordinate(float lat, float lon, float alt, float speed) const {
  return isfinite(lat) && isfinite(lon) && isfinite(alt) && isfinite(speed) &&
         lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f;
}
