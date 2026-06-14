/**
 * keyboard_manager.cpp — 键盘管理器实现
 *
 * 每帧调用scan()扫描键盘状态，检测按键变化并生成KeyEvent。
 * 支持按键保持重复（长按方向键时自动触发连续事件）。
 */
#include "keyboard_manager.h"

KeyboardManager& KeyboardManager::instance() {
  static KeyboardManager km;
  return km;
}

void KeyboardManager::scan() {
  static bool currStates[KEY_STATE_SIZE] = {};  // 当前帧按键状态(static避免栈分配)
  M5Cardputer.update();
  unsigned long now = millis();

  // 清空当前状态
  memset(currStates, 0, sizeof(currStates));

  // 收集当前按键状态
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

  // 处理word中的单个字符按键
  for (auto c : status.word) {
    int idx = (int)(unsigned char)c;
    if (idx >= 0 && idx < KEY_STATE_SIZE) {
      currStates[idx] = true;
    }
  }

  // 处理特殊键
  if (status.enter)  currStates[0x0D] = true;
  if (status.tab)    currStates[0x09] = true;
  if (status.del)    currStates[0x7F] = true;

  // 检测按键变化并生成KeyEvent
  for (int i = 0; i < KEY_STATE_SIZE; i++) {
    if (currStates[i] != _prevKeyStates[i]) {
      KeyEvent event;
      event.key = (char)i;
      event.pressed = currStates[i];
      event.released = !currStates[i];
      event.held = false;
      event.holdDuration = 0;

      if (event.pressed) {
        _keyPressTime[i] = now;
        _keyRepeatArmed[i] = false;
      } else {
        event.holdDuration = now - _keyPressTime[i];
      }

      _lastActivityMs = now;

      // 分发事件（如果被消费则更新状态并停止本轮扫描）
      if (_dispatch(event)) {
        _prevKeyStates[i] = currStates[i];
        return;
      }

      _prevKeyStates[i] = currStates[i];
    } else if (currStates[i]) {
      // 持续按住 — 检查是否需要发送重复事件
      unsigned long held = now - _keyPressTime[i];
      if (!_keyRepeatArmed[i] && held >= holdRepeatFirstDelay()) {
        // 首次进入重复模式
        _keyRepeatArmed[i] = true;
        KeyEvent event;
        event.key = (char)i;
        event.pressed = false;
        event.released = false;
        event.held = true;
        event.holdDuration = held;
        _lastActivityMs = now;
        _dispatch(event);
      }
    }
  }
}

void KeyboardManager::setListener(IKeyListener* listener) {
  _listener = listener;
}

void KeyboardManager::setGlobalInterceptor(IKeyListener* interceptor) {
  _globalInterceptor = interceptor;
}

void KeyboardManager::clearKeyStates() {
  memset(_prevKeyStates, 0, sizeof(_prevKeyStates));
  memset(_keyPressTime, 0, sizeof(_keyPressTime));
  memset(_keyRepeatArmed, 0, sizeof(_keyRepeatArmed));
  _lastActivityMs = millis();
}

bool KeyboardManager::isKeyPressed(char key) const {
  int idx = (int)(unsigned char)key;
  if (idx < 0 || idx >= KEY_STATE_SIZE) return false;
  return _prevKeyStates[idx];
}

bool KeyboardManager::_dispatch(const KeyEvent& event) {
  // 先检查全局拦截器
  if (_globalInterceptor && _globalInterceptor->onKeyEvent(event)) {
    return true;
  }
  // 再分发给当前监听者
  if (_listener && _listener->onKeyEvent(event)) {
    return true;
  }
  return false;
}
