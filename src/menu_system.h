/**
 * menu_system.h — 横向滚动菜单系统
 * 实现主菜单UI：横向滚动、滑动动画、图标绘制、Enter进入/ESC退出
 */
#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <M5Cardputer.h>
#include <vector>
#include "config.h"
#include "keyboard_manager.h"
#include "sub_functions/sub_function.h"

class MenuSystem : public IKeyListener {
public:
  static MenuSystem& instance();
  void begin();
  void draw();
  bool onKeyEvent(const KeyEvent& event) override;
  void enterFunction();
  void exitFunction();
  bool isInMenu() const { return _activeFn == nullptr; }
  SubFunction* activeFunction() const { return _activeFn; }
  int selectedIndex() const { return _selectedIndex; }
  int itemCount() const { return (int)_items.size(); }
  void updateAnimation();

private:
  MenuSystem() = default;
  MenuSystem(const MenuSystem&) = delete;
  MenuSystem& operator=(const MenuSystem&) = delete;

  void _navigateLeft();
  void _navigateRight();
  void _drawMenuItem(int index, int cx, int cy, bool selected);

  std::vector<SubFunction*> _items;
  int _selectedIndex = 0;

  // 动画状态
  int _animDir = 0;            // +1=右移(新项从右边来), -1=左移(新项从左边来), 0=无动画
  float _animProgress = 0;     // 动画进度 0→1
  unsigned long _animStart = 0;
  bool _animating = false;

  SubFunction* _activeFn = nullptr;
};

#endif
