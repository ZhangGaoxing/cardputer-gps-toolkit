/**
 * fn_settings.h — 显示设置页面
 */
#ifndef FN_SETTINGS_H
#define FN_SETTINGS_H

#include "sub_function.h"

class FnSettings : public SubFunction {
public:
  FnSettings() : SubFunction("Settings", ICON_SETTINGS) { setUpdateInterval(200); }

  void onEnter() override;
  void onUpdate(bool force) override;
  bool needsRedraw(unsigned long now) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  void _moveSelection(int delta);
  void _adjustCurrent(int delta);
  void _saveSettings();

  int _selectedRow = 0;
  bool _dirty = true;
  bool _sdReady = false;
};

#endif