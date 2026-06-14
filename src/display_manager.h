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
  enum BrightnessLevel {
    BRIGHTNESS_LOW = 0,
    BRIGHTNESS_MEDIUM,
    BRIGHTNESS_HIGH
  };

  enum SleepTimeoutOption {
    SLEEP_1_MIN = 0,
    SLEEP_5_MIN,
    SLEEP_10_MIN,
    SLEEP_ALWAYS_ON
  };

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

  /** 设置亮度档位 */
  void setBrightnessLevel(uint8_t level);

  /** 获取当前亮度档位 */
  uint8_t brightnessLevel() const { return _brightnessLevel; }

  /** 获取当前亮度值 */
  uint8_t brightnessValue() const;

  /** 设置息屏时间档位 */
  void setSleepTimeoutIndex(uint8_t index);

  /** 获取当前息屏时间档位 */
  uint8_t sleepTimeoutIndex() const { return _sleepTimeoutIndex; }

  /** 获取当前息屏时间(ms)，0表示常亮 */
  unsigned long sleepTimeoutMs() const;

  /** 根据最近交互时间更新屏幕亮灭状态 */
  void updatePowerState(unsigned long now, unsigned long lastActivityMs);

  /** 当前是否处于息屏状态 */
  bool isScreenSleeping() const { return _screenSleeping; }

  /** 亮度档位标签 */
  const char* brightnessLabel() const;

  /** 息屏时间标签 */
  const char* sleepTimeoutLabel() const;

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

  void _applyBrightness();

  M5Canvas* _frameBuf = nullptr;
  uint8_t _brightnessLevel = BRIGHTNESS_MEDIUM;
  uint8_t _sleepTimeoutIndex = SLEEP_5_MIN;
  bool _screenSleeping = false;
};

#endif // DISPLAY_MANAGER_H
