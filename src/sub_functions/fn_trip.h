/**
 * fn_trip.h — 行程（合并 Trip Stats + Trip Track + Record，Tab 切换子页面）
 */
#ifndef FN_TRIP_H
#define FN_TRIP_H
#include "sub_function.h"
class FnTrip : public SubFunction {
public:
  FnTrip() : SubFunction("Trip", ICON_TRIP) { setUpdateInterval(1000); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void onExit() override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  int _subTab = 0;  // 0=Stats, 1=Track, 2=Alt, 3=Speed, 4=Record
  void _drawStats();
  void _drawTrack();
  void _drawAlt();
  void _drawSpeed();
  void _drawRecord();
};
#endif
