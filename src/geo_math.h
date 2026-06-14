/**
 * geo_math.h — 地理计算函数（项目唯一实现）
 *
 * 所有 Haversine 距离和方位角计算集中在此处，
 * 避免各文件重复定义常量和函数。
 */
#ifndef GEO_MATH_H
#define GEO_MATH_H

#include <Arduino.h>

/// 十进制度 → 弧度
#define GEO_DEG_TO_RAD    0.017453292519943295f

/// 弧度 → 十进制度
#define GEO_RAD_TO_DEG    57.29577951308232f

/// 地球平均半径 (km)
#define GEO_EARTH_RADIUS_KM 6371.0f

/// π (float)
#define GEO_M_PI          3.14159265358979323846f

/**
 * Haversine 距离 (km)
 * @param lat1, lon1 点 1 十进制度
 * @param lat2, lon2 点 2 十进制度
 * @return 两点间大圆距离（km），无效坐标返回 NaN
 */
float geoDistanceKm(float lat1, float lon1, float lat2, float lon2);

/**
 * 初始方位角 (0–360°)
 * @param lat1, lon1 起点十进制度
 * @param lat2, lon2 终点十进制度
 * @return 方位角（0–360），相同坐标返回 0，无效坐标返回 NaN
 */
float geoBearingDeg(float lat1, float lon1, float lat2, float lon2);

#endif // GEO_MATH_H
