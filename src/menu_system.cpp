/**
 * menu_system.cpp — 横向滚动菜单系统实现
 *
 * 动画逻辑：
 *   按右键 → 新项从右边滑入（视觉上所有项向左移）
 *   按左键 → 新项从左边滑入（视觉上所有项向右移）
 *
 *   选中项始终固定在屏幕中心 (x = SCREEN_W/2)，不移动。
 *   左右相邻项相对于选中项偏移 MENU_ITEM_SPACING + 动画偏移。
 */
#include "menu_system.h"
#include "display_manager.h"
#include "status_bar.h"
#include "ui_helpers.h"

#include "sub_functions/fn_gps_dashboard.h"
#include "sub_functions/fn_sos.h"
#include "sub_functions/fn_signal_bars.h"
#include "sub_functions/fn_trip.h"
#include "sub_functions/fn_backtrack.h"
#include "sub_functions/fn_gps_clock.h"
#include "sub_functions/fn_3d_globe.h"
#include "sub_functions/fn_world_map.h"
#include "sub_functions/fn_offline_map.h"
#include "sub_functions/fn_peers.h"
#include "sub_functions/fn_waypoint.h"
#include "sub_functions/fn_goto_nav.h"
#include "sub_functions/fn_nmea_monitor.h"
#include "sub_functions/fn_settings.h"
#include "sub_functions/fn_about.h"
#include "sub_functions/fn_compass.h"

MenuSystem& MenuSystem::instance() {
  static MenuSystem ms;
  return ms;
}

void MenuSystem::begin() {
  // 顺序与 ScreenID 保持一致
  _items.push_back(new FnGpsDashboard());
  _items.push_back(new FnSos());
  _items.push_back(new FnSignalBars());
  _items.push_back(new FnTrip());
  _items.push_back(new FnBacktrack());
  _items.push_back(new FnGpsClock());
  _items.push_back(new Fn3DGlobe());
  _items.push_back(new FnWorldMap());
  _items.push_back(new FnOfflineMap());
  _items.push_back(new FnPeers());
  _items.push_back(new FnWaypoint());
  _items.push_back(new FnGotoNav());
  _items.push_back(new FnCompass());
  _items.push_back(new FnNmeaMonitor());
  _items.push_back(new FnSettings());
  _items.push_back(new FnAbout());
}

// ==================================================================
//  导航
// ==================================================================

void MenuSystem::_navigateLeft() {
  if (_animating) return;
  _animFromIndex = _selectedIndex;
  _animToIndex = (_selectedIndex - 1 + (int)_items.size()) % (int)_items.size();
  _animDir = -1;
  _animProgress = 0;
  _animating = true;
  _animStart = millis();
}

void MenuSystem::_navigateRight() {
  if (_animating) return;
  _animFromIndex = _selectedIndex;
  _animToIndex = (_selectedIndex + 1) % (int)_items.size();
  _animDir = +1;
  _animProgress = 0;
  _animating = true;
  _animStart = millis();
}

// ==================================================================
//  动画更新
// ==================================================================

void MenuSystem::updateAnimation() {
  if (!_animating) return;

  unsigned long elapsed = millis() - _animStart;
  float t = (float)elapsed / MENU_ANIM_DURATION;
  if (t >= 1.0f) {
    _animProgress = 1.0f;
    _animating = false;
    _selectedIndex = _animToIndex;
    _animDir = 0;
  } else {
    // ease-out: 1 - (1-t)^2
    _animProgress = 1.0f - (1.0f - t) * (1.0f - t);
  }
}

// ==================================================================
//  菜单绘制
// ==================================================================

void MenuSystem::draw() {
  M5Canvas& cv = DisplayManager::instance().canvas();

  // 状态栏
  StatusBar::instance().draw();

  // 内容区背景 — 深绿主题
  cv.fillRect(0, STATUSBAR_HEIGHT, SCREEN_W, SCREEN_H - STATUSBAR_HEIGHT, UI_BG);

  // 底部操作提示栏背景
  int tipY = SCREEN_H - 20;
  cv.fillRect(0, tipY, SCREEN_W, 20, UI_TIPS_BG);
  cv.drawLine(0, tipY, SCREEN_W - 1, tipY, UI_DIM);

  // 底部提示
  cv.setTextSize(1);
  cv.setTextColor(UI_TEXT);
  cv.setCursor(4, tipY + 4);
  cv.print("[,]/[/]Nav [Ent]Open [E]SOS");

  int currentIndex = _animating ? _animToIndex : _selectedIndex;

  // 页标
  char pageBuf[16];
  snprintf(pageBuf, sizeof(pageBuf), "%d/%d", currentIndex + 1, (int)_items.size());
  int pw = strlen(pageBuf) * 6;
  cv.setTextColor(UI_DIM);
  cv.setCursor(SCREEN_W - pw - 4, tipY + 4);
  cv.print(pageBuf);

  // 圆点指示器 — #00FF00，下移到靠近底部提示栏
  int dotY = tipY - 5;
  int totalDots = (int)_items.size();
  int dotSpacing = 7;
  int dotsWidth = totalDots * dotSpacing;
  int dotStartX = (SCREEN_W - dotsWidth) / 2;
  for (int d = 0; d < totalDots; d++) {
    int dx = dotStartX + d * dotSpacing;
    if (d == currentIndex) {
      cv.fillCircle(dx, dotY, 3, UI_ACTIVE);
    } else {
      cv.drawCircle(dx, dotY, 2, UI_DIM);
    }
  }

  int centerX = SCREEN_W / 2;
  int centerY = STATUSBAR_HEIGHT + (tipY - STATUSBAR_HEIGHT) / 2;

  if (_animating) {
    float animShift = _animDir * _animProgress * MENU_ITEM_SPACING;
    int deltaStart = (_animDir > 0) ? -1 : -2;
    int deltaEnd = (_animDir > 0) ? 2 : 1;

    for (int delta = deltaStart; delta <= deltaEnd; delta++) {
      int idx = (_animFromIndex + delta + (int)_items.size()) % (int)_items.size();
      int cx = centerX + delta * MENU_ITEM_SPACING - (int)animShift;
      bool selected = (idx == _animToIndex && _animProgress >= 0.5f) ||
                      (idx == _animFromIndex && _animProgress < 0.5f);
      int cyOffset = selected ? -4 : MENU_Y_OFFSET;
      _drawMenuItem(idx, cx, centerY + cyOffset, selected);
    }
    return;
  }

  // 绘制选中项 + 左右各1个
  for (int delta = -1; delta <= 1; delta++) {
    int idx = (_selectedIndex + delta + (int)_items.size()) % (int)_items.size();
    bool selected = (delta == 0);

    int cx = centerX + delta * MENU_ITEM_SPACING;
    // 非选中项向下偏移，产生3D弧形效果
    int cyOffset = selected ? -4 : MENU_Y_OFFSET;
    _drawMenuItem(idx, cx, centerY + cyOffset, selected);
  }
}

void MenuSystem::_drawMenuItem(int index, int cx, int cy, bool selected) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  SubFunction* item = _items[index];

  int iconSize = selected ? MENU_ICON_SIZE : MENU_SMALL_ICON;
  uint16_t iconColor = selected ? UI_BRASS : UI_DIM;

  int itemRight = cx + iconSize / 2 + 30;
  int itemLeft  = cx - iconSize / 2 - 30;
  if (itemRight < 0 || itemLeft > SCREEN_W) return;

  int iconX = cx - iconSize / 2;
  int iconY = cy - iconSize / 2;

  drawMenuIcon(cv, item->iconType(), iconX, iconY, iconSize, iconColor);

  // 选中项大号字体
  if (selected) {
    cv.setTextSize(2);
    cv.setTextColor(UI_TEXT);
    int nameW = strlen(item->displayName()) * 12;
    cv.setCursor(cx - nameW / 2, cy + iconSize / 2 + 6);
    cv.print(item->displayName());
  }
}

bool MenuSystem::onKeyEvent(const KeyEvent& event) {
  if (SOS_QUICK_ACCESS_ENABLED && event.held && (event.key == 'e' || event.key == 'E')) {
    _animating = false;
    _animDir = 0;
    _selectedIndex = SCR_SOS;
    enterFunction();
    return true;
  }

  if (!event.pressed) return false;
  if (event.key == ',')  { _navigateLeft();  return true; }
  if (event.key == '/')  { _navigateRight(); return true; }
  if (event.key == 0x0D) { enterFunction();  return true; }
  return false;
}

// ==================================================================
//  进入/退出子功能
// ==================================================================

void MenuSystem::enterFunction() {
  if (_activeFn != nullptr) return;
  _activeFn = _items[_selectedIndex];
  KeyboardManager::instance().clearKeyStates();
  _activeFn->onEnter();
  KeyboardManager::instance().setListener(_activeFn);
  DisplayManager::instance().clearScreen(TFT_BLACK);
  _activeFn->onUpdate(true);
  DisplayManager::instance().commit();
}

void MenuSystem::exitFunction() {
  if (_activeFn == nullptr) return;
  _activeFn->onExit();
  _activeFn = nullptr;
  KeyboardManager::instance().clearKeyStates();
  KeyboardManager::instance().setListener(this);
  DisplayManager::instance().clearScreen(TFT_BLACK);
  draw();
  DisplayManager::instance().commit();
}
