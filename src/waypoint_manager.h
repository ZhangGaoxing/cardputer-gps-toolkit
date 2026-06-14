/**
 * waypoint_manager.h — 航点管理器
 *
 * 管理航点的增删改查、持久化存储和导航计算。
 * 使用 CSV 格式存储在 SD 卡上，支持容错加载和原子写入。
 */
#ifndef WAYPOINT_MANAGER_H
#define WAYPOINT_MANAGER_H

#include <Arduino.h>
#include "config.h"

// ==================================================================
//  Waypoint 数据结构
// ==================================================================
struct Waypoint {
  uint16_t  id;                           // 唯一ID（递增）
  char      name[WAYPOINT_NAME_MAX_LEN];  // 名称（最大16字符）
  float     lat;                          // 纬度（十进制度，-90 ~ 90）
  float     lon;                          // 经度（十进制度，-180 ~ 180）
  float     ele;                          // 海拔（米，0表示无效）
  int       year, month, day;             // 创建日期（UTC）
  int       hour, minute, second;         // 创建时间（UTC）
  int       updYear, updMonth, updDay;    // 更新日期（UTC）
  int       updHour, updMinute, updSecond;// 更新时间（UTC）
  uint8_t   source;                       // WaypointSource 枚举值
  uint8_t   type;                         // WaypointType 枚举值
  char      note[WAYPOINT_NOTE_MAX_LEN];  // 备注（最大32字符）
};

// ==================================================================
//  WaypointManager 单例
// ==================================================================
class WaypointManager {
public:
  static WaypointManager& instance();

  /**
   * 初始化管理器。
   * 如果 SD 卡可用则自动加载航点文件。
   * @return true 如果初始化成功
   */
  bool begin();

  /**
   * 从 SD 卡重新加载航点列表。
   * 加载时会跳过损坏行并记录错误数。
   * @return true 如果加载成功（文件不存在也算成功）
   */
  bool load();

  /**
   * 将航点列表保存到 SD 卡。
   * 使用原子写入：先写 .tmp，成功后再替换正式文件。
   * @return true 如果保存成功
   */
  bool save();

  // ======== CRUD 操作 ========

  /**
   * 添加一个航点。自动分配 ID 并设置创建/更新时间。
   * @param year, month, day, hour, minute, second  UTC 时间（GPS 提供）
   * @return 新航点指针，失败返回 nullptr
   */
  const Waypoint* addWaypoint(const char* name, float lat, float lon, float ele,
                              WaypointSource source, WaypointType type = WP_TYPE_CUSTOM,
                              const char* note = "",
                              int year = 0, int month = 0, int day = 0,
                              int hour = 0, int minute = 0, int second = 0);

  /**
   * 删除指定 ID 的航点。
   * @return true 如果删除成功
   */
  bool deleteWaypoint(uint16_t id);

  /**
   * 重命名指定 ID 的航点。
   * @return true 如果重命名成功
   */
  bool renameWaypoint(uint16_t id, const char* newName);

  /**
   * 更新航点全部字段（保持 ID 不变）。
   * @return true 如果更新成功
   */
  bool updateWaypoint(uint16_t id, const char* name, float lat, float lon, float ele,
                      WaypointSource source, WaypointType type, const char* note);

  /**
   * 更新航点的备注。
   */
  bool updateNote(uint16_t id, const char* newNote);

  /**
   * 更新航点的类型。
   */
  bool updateType(uint16_t id, WaypointType newType);

  // ======== 查询操作 ========

  /** 当前航点数量 */
  size_t count() const { return _count; }

  /** 最大航点数量 */
  size_t maxCount() const { return WAYPOINT_MAX_COUNT; }

  /** 是否已满 */
  bool isFull() const { return _count >= WAYPOINT_MAX_COUNT; }

  /** 按索引获取航点（0-based） */
  const Waypoint* getByIndex(size_t index) const;

  /** 按 ID 获取航点 */
  const Waypoint* getById(uint16_t id) const;

  /** 查找最近的航点索引，-1表示无航点 */
  int findNearest(float lat, float lon) const;

  /** 计算航点到当前位置的距离（km） */
  float distanceToWaypoint(uint16_t id, float currentLat, float currentLon) const;

  /** 计算航点到当前位置的初始方位角（0-360度） */
  float bearingToWaypoint(uint16_t id, float currentLat, float currentLon) const;

  /** 计算两点间距离（km） */
  static float distanceKm(float lat1, float lon1, float lat2, float lon2);

  /** 计算两点间初始方位角（0-360度） */
  static float bearingDeg(float lat1, float lon1, float lat2, float lon2);

  /** 获取下一个自动 ID */
  uint16_t nextId() const { return _nextId; }

  /** 加载时跳过的损坏行数 */
  int loadErrors() const { return _loadErrors; }

  /** 获取最后一次错误信息 */
  const char* lastError() const { return _lastError; }

private:
  WaypointManager() = default;
  WaypointManager(const WaypointManager&) = delete;
  WaypointManager& operator=(const WaypointManager&) = delete;

  bool _autoSave();
  bool _rollbackFailedSave();
  void _setError(const char* msg);
  void _clearError();
  int _findIndexById(uint16_t id) const;

  // CSV 行解析和格式化
  static bool _parseCsvLine(const String& line, Waypoint& wp);
  static void _formatCsvLine(const Waypoint& wp, String& out);

  // 字符串 CSV 转义
  static String _csvEscape(const char* str);
  static void _csvUnescape(String& str);

  Waypoint _waypoints[WAYPOINT_MAX_COUNT];
  size_t   _count = 0;
  uint16_t _nextId = 1;
  bool     _dirty = false;        // 自上次保存后是否有修改
  bool     _sdReady = false;
  int      _loadErrors = 0;
  char     _lastError[48] = "";
};

#endif // WAYPOINT_MANAGER_H
