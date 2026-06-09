#ifndef FN_GPS_CLOCK_H
#define FN_GPS_CLOCK_H
#include "sub_function.h"
class FnGpsClock : public SubFunction {
public:
  FnGpsClock() : SubFunction("Clock", ICON_CLOCK) {}
  void onEnter() override;
  void onUpdate(bool force) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
};
#endif
