/**
 * fn_gps_dashboard.h — GPS仪表盘（整合 Dashboard + Fix Info + Coords，Tab 切换）
 */
#ifndef FN_GPS_DASHBOARD_H
#define FN_GPS_DASHBOARD_H
#include "sub_function.h"

class FnGpsDashboard : public SubFunction {
public:
  FnGpsDashboard() : SubFunction("Dashboard", ICON_DASHBOARD) {}
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  int _tab = 0;
  void _drawTabMain();
  void _drawTabFix();
  void _drawTabCoord();
  void _drawSkyPlot(int px, int py, int pw, int ph);
};

#endif
