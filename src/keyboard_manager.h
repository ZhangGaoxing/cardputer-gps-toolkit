/**
 * keyboard_manager.h — 键盘管理器
 * 集中扫描Cardputer矩阵键盘，生成KeyEvent并分发给当前监听者
 */
#ifndef KEYBOARD_MANAGER_H
#define KEYBOARD_MANAGER_H

#include <M5Cardputer.h>
#include "config.h"

// ==================================================================
//  按键事件监听者接口
// ==================================================================
class IKeyListener {
public:
  /**
   * 按键事件回调
   * @param event 按键事件
   * @return true=事件已消费不再传递，false=未处理
   */
  virtual bool onKeyEvent(const KeyEvent& event) = 0;
  virtual ~IKeyListener() = default;
};

// ==================================================================
//  KeyboardManager 单例
// ==================================================================
class KeyboardManager {
public:
  /** 获取单例实例 */
  static KeyboardManager& instance();

  /**
   * 每帧调用一次，扫描键盘并分发事件
   * 先检查全局拦截器（如ESC退出），再分发给当前监听者
   */
  void scan();

  /** 设置当前按键监听者（菜单或子功能） */
  void setListener(IKeyListener* listener);

  /** 获取当前监听者 */
  IKeyListener* currentListener() const { return _listener; }

  /**
   * 设置全局按键拦截器（优先级最高，始终先检查）
   * 用于实现ESC/backtick全局退出功能
   */
  void setGlobalInterceptor(IKeyListener* interceptor);

  /** 清除所有按键状态（在切换子功能时调用，防止按键粘连） */
  void clearKeyStates();

  /** 检查指定按键是否刚按下 */
  bool isKeyPressed(char key) const;

  /** 最近一次键盘活动时间(ms) */
  unsigned long lastActivityMillis() const { return _lastActivityMs; }

  /** 获取方向键重复的第一步延迟(ms)，首次按下后等待 */
  unsigned long holdRepeatFirstDelay() const { return 400; }

  /** 获取方向键重复间隔(ms)，首次等待后的连续重复间隔 */
  unsigned long holdRepeatInterval() const { return DIR_REPEAT_MS; }

private:
  KeyboardManager() = default;
  KeyboardManager(const KeyboardManager&) = delete;
  KeyboardManager& operator=(const KeyboardManager&) = delete;

  IKeyListener* _listener = nullptr;
  IKeyListener* _globalInterceptor = nullptr;

  // 按键状态跟踪
  static const int KEY_STATE_SIZE = 128;
  bool _prevKeyStates[KEY_STATE_SIZE] = {};         // 上一帧的按键状态
  unsigned long _keyPressTime[KEY_STATE_SIZE] = {}; // 按键按下的时间戳
  bool _keyRepeatArmed[KEY_STATE_SIZE] = {};        // 是否已进入重复模式
  unsigned long _lastActivityMs = 0;

  /**
   * 将KeyEvent分发给全局拦截器和当前监听者
   * @return true=事件已被消费
   */
  bool _dispatch(const KeyEvent& event);
};

#endif // KEYBOARD_MANAGER_H
