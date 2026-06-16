/**
 * battery_manager.cpp — 电池电量管理器实现
 */
#include "battery_manager.h"
#include <M5Cardputer.h>

BatteryManager& BatteryManager::instance() {
  static BatteryManager bm;
  return bm;
}

void BatteryManager::begin() {
  _readNow();
}

void BatteryManager::update() {
  unsigned long now = millis();
  if (now - _lastRead < READ_INTERVAL) return;
  _readNow();
}

void BatteryManager::_readNow() {
  _lastRead = millis();
  // 使用M5Cardputer的电源管理API读取电量
  _percentage = M5Cardputer.Power.getBatteryLevel();

  // 限制在0-100范围
  if (_percentage < 0)   _percentage = 0;
  if (_percentage > 100) _percentage = 100;

  // 检测充电状态（如果API支持）
  _charging = M5Cardputer.Power.isCharging();
}
