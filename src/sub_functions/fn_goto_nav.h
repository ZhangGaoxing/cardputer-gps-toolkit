/**
 * fn_goto_nav.h - Go-to waypoint navigation page.
 */
#ifndef FN_GOTO_NAV_H
#define FN_GOTO_NAV_H

#include "sub_function.h"

class FnGotoNav : public SubFunction {
public:
  FnGotoNav() : SubFunction("Go-to", ICON_GOTO_NAV) { setUpdateInterval(500); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  void _drawNoTarget();
  void _drawActive();
};

#endif // FN_GOTO_NAV_H
