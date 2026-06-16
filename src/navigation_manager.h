/**
 * navigation_manager.h - runtime Go-to waypoint navigation state.
 */
#ifndef NAVIGATION_MANAGER_H
#define NAVIGATION_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "waypoint_manager.h"

struct NavigationState {
  bool active = false;
  uint16_t targetId = 0;
  char targetName[WAYPOINT_NAME_MAX_LEN] = "";
  float targetLat = 0.0f;
  float targetLon = 0.0f;
  float targetEle = 0.0f;

  bool dataAvailable = false;
  float distanceM = NAN;
  float bearingDeg = NAN;
  float headingDeg = NAN;
  float relativeBearingDeg = NAN;
  bool etaAvailable = false;
  uint32_t etaSeconds = 0;
  bool arrived = false;
  char lastError[48] = "";
};

class NavigationManager {
public:
  static NavigationManager& instance();

  void begin();
  void update();

  bool startGoto(uint16_t waypointId);
  void stop(const char* reason = nullptr);

  bool isActive() const { return _state.active; }
  bool isTarget(uint16_t waypointId) const {
    return _state.active && _state.targetId == waypointId;
  }
  uint16_t targetId() const { return _state.targetId; }
  const NavigationState& state() const { return _state; }
  const char* lastError() const { return _state.lastError; }

  static bool isValidCoordinate(float lat, float lon);
  static float normalize360(float deg);
  static float normalizeRelative(float deg);
  static const char* relativeText(float relDeg, bool headingValid);

private:
  NavigationManager() = default;
  NavigationManager(const NavigationManager&) = delete;
  NavigationManager& operator=(const NavigationManager&) = delete;

  void _clearComputed();
  void _setError(const char* msg);
  bool _loadTarget();
  void _saveTarget();
  void _clearSavedTarget();

  NavigationState _state;
  float _smoothedEtaSec = NAN;
};

#endif // NAVIGATION_MANAGER_H
