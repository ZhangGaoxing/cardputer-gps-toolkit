/**
 * ui_helpers.h — 共享UI绘制辅助函数
 * 提供各子功能公用的绘图函数
 */
#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <M5Cardputer.h>

/** 卫星系统→颜色映射 */
uint16_t systemColor(const String& sys);

/** 方位角→方位名(N/NE/E/SE/S/SW/W/NW) */
const char* cardinalFromHeading(float heading);

/** 方位角→目标方位 */
float bearingTo(float lat1, float lon1, float lat2, float lon2);

/** Haversine距离(km) */
float haversineKm(float lat1, float lon1, float lat2, float lon2);

/** DOP值→质量标签 */
const char* dopQuality(float dop);

#endif
