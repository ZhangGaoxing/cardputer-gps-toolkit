/**
 * display_manager.h — 显示管理器
 * 管理M5Canvas离屏帧缓冲和屏幕刷新
 */
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <M5Cardputer.h>
#include "config.h"

class DisplayManager {
public:
  /** 获取单例实例 */
  static DisplayManager& instance();

  /** 初始化显示（创建画布精灵、设置旋转和亮度） */
  void begin();

  /** 获取离屏帧缓冲画布引用 */
  M5Canvas& canvas() { return *_frameBuf; }

  /** 用指定颜色填充整个画布 */
  void clearScreen(uint16_t color = TFT_BLACK);

  /** 将画布内容推送到物理屏幕 */
  void commit();

  /** 获取屏幕宽度 */
  int screenWidth()  const { return SCREEN_W; }

  /** 获取屏幕高度 */
  int screenHeight() const { return SCREEN_H; }

  /** 获取状态栏下方内容区域的Y起始坐标 */
  int contentTop() const { return CONTENT_TOP; }

  /** 获取内容区域高度 */
  int contentHeight() const { return CONTENT_H; }

private:
  DisplayManager() = default;
  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;
  ~DisplayManager();

  M5Canvas* _frameBuf = nullptr;
};

#endif // DISPLAY_MANAGER_H
