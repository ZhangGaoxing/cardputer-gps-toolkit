/**
 * fn_waypoint.h — 航点导航
 */
#ifndef FN_WAYPOINT_H
#define FN_WAYPOINT_H
#include "sub_function.h"
#include <Arduino.h>

class FnWaypoint : public SubFunction {
public:
  FnWaypoint() : SubFunction("Waypoint", ICON_WAYPOINT) { setUpdateInterval(500); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  bool _waypointSet = false;
  bool _editing = false;
  int _field = 0;
  String _tmp[2] = {"", ""};
  float _waypointLat = 0, _waypointLon = 0;
};

#endif
