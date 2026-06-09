/**
 * status_bar.h — 顶部状态栏
 * 左上角显示RTC时间，右上角显示电池电量
 */
#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <M5Cardputer.h>
#include "config.h"

class StatusBar {
public:
  /** 获取单例实例 */
  static StatusBar& instance();

  /** 绘制状态栏到当前画布 */
  void draw();

  /** 状态栏高度 */
  int height() const { return STATUSBAR_HEIGHT; }

private:
  StatusBar() = default;
  StatusBar(const StatusBar&) = delete;
  StatusBar& operator=(const StatusBar&) = delete;

  /** 绘制时间（左上角） */
  void _drawTime(int x, int y);

  /** 绘制电池图标（右上角） */
  void _drawBattery(int x, int y);
};

#endif // STATUS_BAR_H
