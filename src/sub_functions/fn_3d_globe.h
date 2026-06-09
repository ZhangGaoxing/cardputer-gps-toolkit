/**
 * fn_3d_globe.h — 3D旋转地球（从 SD 卡读取低分辨率矢量数据）
 */
#ifndef FN_3D_GLOBE_H
#define FN_3D_GLOBE_H
#include "sub_function.h"
#include "../sd_manager.h"

class Fn3DGlobe : public SubFunction {
public:
  Fn3DGlobe() : SubFunction("3D Globe", ICON_3D_GLOBE) { setUpdateInterval(50); }
  void onEnter() override;
  void onExit() override;
  void onUpdate(bool force) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  float _camAngle = 0;
  unsigned long _lastMs = 0;
  bool _sdReady = false;
  VectorReader _coastLowReader;
};

#endif
