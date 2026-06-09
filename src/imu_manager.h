/**
 * imu_manager.h — IMU传感器管理器
 * 【Phase 2将完整移植GPSInfo的BMI270代码】
 */
#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#include <Arduino.h>

class IMUManager {
public:
  static IMUManager& instance();
  void begin();
  void update();
  bool isAvailable() const { return _available; }
  float pitch() const       { return _pitch; }
  float roll() const        { return _roll; }
  float gForce() const      { return _gForce; }
  float temperature() const { return _temp; }
  float maxGForce() const   { return _maxG; }
private:
  IMUManager() = default;
  bool _available = false;
  float _ax=0, _ay=0, _az=0, _gx=0, _gy=0, _gz=0;
  float _pitch=0, _roll=0, _gForce=1.0, _temp=0, _maxG=1.0;
};

#endif
