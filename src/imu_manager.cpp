#include "imu_manager.h"
#include <M5Cardputer.h>
#include <math.h>

IMUManager& IMUManager::instance() { static IMUManager im; return im; }

void IMUManager::begin() {
  if (M5.Imu.isEnabled()) {
    _available = true;
    M5.Imu.loadOffsetFromNVS();
  }
}

void IMUManager::update() {
  if (!_available) return;
  M5.Imu.getAccel(&_ax, &_ay, &_az);
  M5.Imu.getGyro(&_gx, &_gy, &_gz);
  M5.Imu.getTemp(&_temp);
  _pitch = atan2(-_ax, sqrt(_ay*_ay + _az*_az)) * 180.0/M_PI;
  _roll  = atan2(_ay, _az) * 180.0/M_PI;
  _gForce = sqrt(_ax*_ax + _ay*_ay + _az*_az);
  if (_gForce > _maxG) _maxG = _gForce;
}
