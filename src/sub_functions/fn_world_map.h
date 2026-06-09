/**
 * fn_world_map.h — 矢量世界地图（从 SD 卡读取矢量数据）
 */
#ifndef FN_WORLD_MAP_H
#define FN_WORLD_MAP_H
#include "sub_function.h"
#include "../sd_manager.h"

class FnWorldMap : public SubFunction {
public:
  FnWorldMap() : SubFunction("World Map", ICON_WORLD_MAP) { setUpdateInterval(500); }
  void onEnter() override;
  void onExit() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  int _zoom = 0;
  bool _sdReady = false;
  VectorReader _coastReader, _borderReader, _stateReader, _riverReader, _lakeReader;
};

#endif
