/**
 * geo_math.cpp — 地理计算函数实现
 *
 * Haversine 距离公式（float 精度，典型误差 < 0.5%）
 * 初始方位角公式（球面三角，精度与 float 一致）
 */
#include "geo_math.h"
#include <math.h>

// ==================================================================
//  Haversine 距离
// ==================================================================

float geoDistanceKm(float lat1, float lon1, float lat2, float lon2) {
  float dLat = (lat2 - lat1) * GEO_DEG_TO_RAD;
  float dLon = (lon2 - lon1) * GEO_DEG_TO_RAD;
  float la1  = lat1 * GEO_DEG_TO_RAD;
  float la2  = lat2 * GEO_DEG_TO_RAD;
  float sinDLatHalf = sinf(dLat * 0.5f);
  float sinDLonHalf = sinf(dLon * 0.5f);
  float a = sinDLatHalf * sinDLatHalf +
            cosf(la1) * cosf(la2) * sinDLonHalf * sinDLonHalf;
  // 防止浮点误差导致 a > 1.0，sqrtf(1-a) 变为 NaN
  if (a > 1.0f) a = 1.0f;
  return GEO_EARTH_RADIUS_KM * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// ==================================================================
//  初始方位角
// ==================================================================

float geoBearingDeg(float lat1, float lon1, float lat2, float lon2) {
  // 相同坐标 → 0°（避免 atan2(0,0) 未定义行为）
  if (lat1 == lat2 && lon1 == lon2) return 0.0f;

  float dLon = (lon2 - lon1) * GEO_DEG_TO_RAD;
  float la1  = lat1 * GEO_DEG_TO_RAD;
  float la2  = lat2 * GEO_DEG_TO_RAD;
  float y = sinf(dLon) * cosf(la2);
  float x = cosf(la1) * sinf(la2) - sinf(la1) * cosf(la2) * cosf(dLon);
  return fmodf(atan2f(y, x) * GEO_RAD_TO_DEG + 360.0f, 360.0f);
}
