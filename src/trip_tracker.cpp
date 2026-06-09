#include "trip_tracker.h"
#include "ui_helpers.h"

TripTracker& TripTracker::instance() { static TripTracker tt; return tt; }

void TripTracker::begin() {
  memset(_buf,0,sizeof(_buf));
  _head=_count=0;
  _stats=TripStats();
  _lastRecord=0;
}

void TripTracker::update(float lat,float lon,float alt,float speed,bool locValid) {
  if(!locValid) return;

  // 初始化
  if(!_stats.hasPrev) {
    _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=alt;
    _stats.hasPrev=true; _stats.startMillis=millis();
    _stats.lastMovingCheck=millis();
    _stats.minAltM=_stats.maxAltM=alt;
    return;
  }

  // 距离
  float dist=haversineKm(_stats.prevLat,_stats.prevLon,lat,lon);
  if(dist>0.001) _stats.totalDistKm+=dist;

  // 极值
  if(speed>_stats.maxSpeedKmph) _stats.maxSpeedKmph=speed;
  if(alt>_stats.maxAltM) _stats.maxAltM=alt;
  if(alt<_stats.minAltM) _stats.minAltM=alt;

  // 升降
  float dAlt=alt-_stats.prevAlt;
  if(dAlt>0.5) _stats.totalAscentM+=dAlt;
  else if(dAlt<-0.5) _stats.totalDescentM-=dAlt;

  // 移动时间
  uint32_t now=millis();
  if(speed>1.0) _stats.movingMillis+=(now-_stats.lastMovingCheck);
  _stats.lastMovingCheck=now;
  _stats.prevLat=lat; _stats.prevLon=lon; _stats.prevAlt=alt;

  // 轨迹点（2秒间隔）
  if(now-_lastRecord>=TRACK_RECORD_INTERVAL) {
    _lastRecord=now;
    TrackPoint& p=_buf[_head];
    p.lat=lat; p.lon=lon; p.altM=alt; p.speedKmph=speed; p.timestamp=now;
    _head=(_head+1)%TRACK_MAX;
    if(_count<TRACK_MAX) _count++;
  }
}

float TripTracker::totalDistanceKm() const { return _stats.totalDistKm; }
