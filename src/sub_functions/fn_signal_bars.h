/**
 * fn_signal_bars.h — 卫星信噪比柱状图 + 星座统计（Tab 切换）
 */
#ifndef FN_SIGNAL_BARS_H
#define FN_SIGNAL_BARS_H
#include "sub_function.h"
class FnSignalBars : public SubFunction {
public:
  FnSignalBars() : SubFunction("Signal", ICON_SIGNAL_BARS) {}
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  int _tab = 0;       // 0=SNR bars, 1=Constellation
  int _scrollX = 0;   // 水平滚动偏移（像素）
  void _drawBars();
  void _drawConstellation();
};
#endif
