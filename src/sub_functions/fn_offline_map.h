/**
 * fn_offline_map.h — 离线瓦片地图
 */
#ifndef FN_OFFLINE_MAP_H
#define FN_OFFLINE_MAP_H
#include "sub_function.h"
class FnOfflineMap : public SubFunction {
public:
  FnOfflineMap() : SubFunction("Offline Map", ICON_OFFLINE_MAP) { setUpdateInterval(300); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  void _centerOnGps();
  void _panByPixels(int dx, int dy);
  void _zoomBy(int delta);
  double _lat = 0, _lon = 0;
  double _gpsLat = 0, _gpsLon = 0;
  int _zoom = ZOOM_DEFAULT;
  bool _sdReady = false;
  bool _hasPosition = false;
  bool _hasGpsPosition = false;
  bool _centeredOnGps = false;
  bool _userPanned = false;
};
#endif
