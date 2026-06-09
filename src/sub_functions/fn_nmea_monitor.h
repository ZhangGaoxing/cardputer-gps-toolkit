#ifndef FN_NMEA_MONITOR_H
#define FN_NMEA_MONITOR_H
#include "sub_function.h"
class FnNmeaMonitor : public SubFunction {
public:
  FnNmeaMonitor() : SubFunction("NMEA", ICON_NMEA) { setUpdateInterval(200); }
  void onEnter() override;
  void onUpdate(bool force) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
};
#endif
