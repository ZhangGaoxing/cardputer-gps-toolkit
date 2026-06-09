/**
 * fn_about.h — 关于页面
 */
#ifndef FN_ABOUT_H
#define FN_ABOUT_H
#include "sub_function.h"
class FnAbout : public SubFunction {
public:
  FnAbout() : SubFunction("About", ICON_ABOUT) { setUpdateInterval(2000); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override { return false; }
  void drawIcon(int x, int y, int size, uint16_t color) override;
};
#endif
