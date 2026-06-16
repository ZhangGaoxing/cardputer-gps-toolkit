#ifndef FN_SOS_H
#define FN_SOS_H

#include "sub_function.h"

class FnSos : public SubFunction {
public:
  FnSos() : SubFunction("SOS", ICON_SOS) { setUpdateInterval(250); }

  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  int _page = 0;
  void _drawHeader(const char* title);
  void _drawPageCore();
  void _drawPageNav();
  void _drawPagePayload();
};

#endif // FN_SOS_H