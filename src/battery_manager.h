/**
 * battery_manager.h — 电池电量管理器
 * 读取M5Stack内置电池电量并计算百分比
 */
#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>

/** 低电量告警级别（改进 E） */
enum class BatteryAlert {
  None,        // >20%，正常
  Low20,       // <=20%，低电量警告（建议开启 GPS 省电）
  Critical10   // <=10%，严重低电（建议立即结束操作）
};

class BatteryManager {
public:
  /** 获取单例实例 */
  static BatteryManager& instance();

  /** 初始化 */
  void begin();

  /** 每10秒更新一次电池读数 */
  void update();

  /** 获取电池电量百分比(0-100) */
  int percentage() const { return _percentage; }

  /** 是否正在充电 */
  bool isCharging() const { return _charging; }

  /** 低电量告警级别 */
  BatteryAlert alertLevel() const;

private:
  BatteryManager() = default;
  BatteryManager(const BatteryManager&) = delete;
  BatteryManager& operator=(const BatteryManager&) = delete;

  void _readNow();

  int _percentage = 100;
  bool _charging = false;
  unsigned long _lastRead = 0;
  static const unsigned long READ_INTERVAL = 10000;  // 10秒读取一次
};

#endif // BATTERY_MANAGER_H
