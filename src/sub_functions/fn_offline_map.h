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
  double _lat = 0, _lon = 0;
  int _zoom = ZOOM_DEFAULT;
  bool _sdReady = false;
  int _panX = 0, _panY = 0;
};
#endif
