/**
 * fn_compass.h — 运动指南针
 *
 * 数据源优先级：
 *   1. GPS COG（速度 ≥ 0.5 m/s 时，为主数据源）
 *   2. 陀螺仪偏航积分（GPS 无效 / 低速时的短期辅助，会漂移）
 *
 * 硬件说明：BMI270 无磁力计，静止时无法感知真北方向。
 */
#ifndef FN_COMPASS_H
#define FN_COMPASS_H

#include "sub_function.h"

class FnCompass : public SubFunction {
public:
  FnCompass() : SubFunction("Compass", ICON_COMPASS) { setUpdateInterval(200); }

  void onEnter() override;
  void onExit() override;
  void onUpdate(bool force) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  // 融合航向角（°，0=北，顺时针）
  float   _heading      = 0.0f;
  // 上一次采用 GPS COG 的值（用于陀螺积分基准）
  float   _headingBase  = 0.0f;
  // 陀螺仪偏航积分时间戳
  unsigned long _lastGyroMs = 0;
  // 当前数据来源状态
  enum HeadingSource { SRC_NONE, SRC_GPS, SRC_GYRO } _source = SRC_NONE;

  // 绘制完整指南针界面
  void _draw();
  // 绘制指南针圆盘 + 指针
  void _drawRose(M5Canvas& cv, int cx, int cy, int r, float heading);
};

#endif
