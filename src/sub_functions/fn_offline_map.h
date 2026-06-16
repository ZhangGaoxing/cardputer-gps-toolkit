/**
 * fn_offline_map.h - offline JPEG tile map.
 */
#ifndef FN_OFFLINE_MAP_H
#define FN_OFFLINE_MAP_H

#include "sub_function.h"
#include "../sd_manager.h"

class FnOfflineMap : public SubFunction {
public:
  FnOfflineMap() : SubFunction("Offline Map", ICON_OFFLINE_MAP) { setUpdateInterval(50); }
  void onEnter() override;
  void onExit() override;
  void onUpdate(bool force) override;
  bool needsRedraw(unsigned long now) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  void _centerOnGps();
  void _panByPixels(int dx, int dy);
  void _zoomBy(int delta);
  void _savePositionIfDue(bool force);
  void _drawMissingTilePlaceholder(int sx, int sy, int z, int x, int y, TileLoadStatus status);
  void _drawWaypoints();
  void _drawNavigationOverlay();
  void _drawBacktrackOverlay();
  void _createWaypointFromCenter();

  double _lat = 0, _lon = 0;
  double _gpsLat = 0, _gpsLon = 0;
  int _zoom = ZOOM_DEFAULT;
  int _maxZoom = ZOOM_MAX;
  bool _sdReady = false;
  bool _hasPosition = false;
  bool _hasGpsPosition = false;
  bool _followGps = true;
  bool _userPanned = false;
  bool _needsRedraw = true;
  bool _lastNavActive = false;
  bool _lastNavArrived = false;
  uint16_t _lastNavTargetId = 0;
  float _lastNavBearingDeg = NAN;
  bool _lastBacktrackActive = false;
  bool _lastBacktrackArrived = false;
  bool _lastBacktrackOffTrack = false;
  size_t _lastBacktrackTargetIndex = 0;
  float _lastBacktrackBearingDeg = NAN;
  bool _positionDirty = false;
  unsigned long _lastSaveMs = 0;
  unsigned long _lastWpFeedbackMs = 0;
  char _wpFeedback[32] = "";
};

#endif // FN_OFFLINE_MAP_H
