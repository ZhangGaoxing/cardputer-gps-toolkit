/**
 * fn_compass.cpp — 运动指南针实现
 *
 * 布局（240×135）：
 *   左侧  — 指南针圆盘（圆心 67,67，半径 55）
 *   右侧  — 航向度数、基数方向、速度、数据源状态
 */
#include "fn_compass.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../imu_manager.h"
#include "../ui_helpers.h"
#include <math.h>

// 指南针圆心与半径（所有派生半径在 _drawRose 内按比例计算）
static const int COMPASS_CX = 65;
static const int COMPASS_CY = 70;
static const int COMPASS_R  = 52;

// 速度阈值：超过此值使用 GPS COG
static const float GPS_SPEED_THRESHOLD_MPS = 0.5f;

static float normalizeAngle(float a) {
  a = fmodf(a, 360.0f);
  if (a < 0.0f) a += 360.0f;
  return a;
}

// ================================================================
//  生命周期
// ================================================================

void FnCompass::onEnter() {
  _source      = SRC_NONE;
  _heading     = 0.0f;
  _headingBase = 0.0f;
  _lastGyroMs  = millis();
}

void FnCompass::onExit() {
  // 无需清理
}

void FnCompass::onUpdate(bool /*force*/) {
  GPSManager& gps = GPSManager::instance();
  IMUManager& imu = IMUManager::instance();

  unsigned long now = millis();
  float dt = (now - _lastGyroMs) / 1000.0f;
  if (dt > 1.0f) dt = 1.0f;   // 防止首帧大跳
  _lastGyroMs = now;

  bool gpsCourseOk = gps.courseValid()
                     && gps.speedValid()
                     && gps.speedMps() >= GPS_SPEED_THRESHOLD_MPS;

  if (gpsCourseOk) {
    // 主数据源：GPS COG
    _heading     = normalizeAngle(gps.courseDeg());
    _headingBase = _heading;
    _source      = SRC_GPS;
  } else if (imu.isAvailable() && _source != SRC_NONE) {
    // 辅助：陀螺仪偏航积分（只在已有基准航向时使用）
    // BMI270 yawRate 单位为 °/s，正值顺时针
    _heading = normalizeAngle(_headingBase + imu.yawRate() * dt);
    _headingBase = _heading;
    _source = SRC_GYRO;
  } else {
    _source = SRC_NONE;
  }

  _draw();
}

// ================================================================
//  绘制
// ================================================================

void FnCompass::_drawRose(M5Canvas& cv, int cx, int cy, int r, float heading) {
  int rTickLong  = r - 8;       // 主刻度内端（基数方向）
  int rTickShort = r - 5;       // 次刻度内端
  int rLabel     = r * 3 / 4;  // 标签半径（约 75%）
  int rNeedle    = r - 9;       // 指针尖
  int rTail      = r / 3;       // 指针尾

  // 外圆环
  cv.drawCircle(cx, cy, r,     TFT_DARKGREY);
  cv.drawCircle(cx, cy, r - 1, TFT_DARKGREY);

  // 16 刻度线（每 22.5°）
  for (int i = 0; i < 16; i++) {
    float a  = i * 22.5f * ((float)M_PI / 180.0f);
    float sa = sinf(a), ca = cosf(a);
    bool isCardinal = (i % 4 == 0);
    int  rIn = isCardinal ? rTickLong : rTickShort;
    cv.drawLine(cx + (int)(sa * rIn),     cy - (int)(ca * rIn),
                cx + (int)(sa * (r - 2)), cy - (int)(ca * (r - 2)),
                isCardinal ? TFT_LIGHTGREY : TFT_DARKGREY);
  }

  // NSEW 标签
  const char* labels[] = { "N", "E", "S", "W" };
  const float angles[]  = { 0.0f, 90.0f, 180.0f, 270.0f };
  cv.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    float a = angles[i] * ((float)M_PI / 180.0f);
    cv.setTextColor(i == 0 ? TFT_RED : TFT_WHITE);
    cv.setCursor(cx + (int)(sinf(a) * rLabel) - 3,
                 cy - (int)(cosf(a) * rLabel) - 4);
    cv.print(labels[i]);
  }

  // 指针（heading 方向 = 红色北端，对面 = 白色尾）
  float hRad  = heading * ((float)M_PI / 180.0f);
  int   tipX  = cx + (int)(sinf(hRad) * rNeedle);
  int   tipY  = cy - (int)(cosf(hRad) * rNeedle);
  int   tailX = cx - (int)(sinf(hRad) * rTail);
  int   tailY = cy + (int)(cosf(hRad) * rTail);
  float pRad  = hRad + (float)M_PI / 2.0f;
  int   needleW = 4;
  int   mlX   = cx + (int)(sinf(pRad) * needleW);
  int   mlY   = cy - (int)(cosf(pRad) * needleW);
  int   mrX   = cx - (int)(sinf(pRad) * needleW);
  int   mrY   = cy + (int)(cosf(pRad) * needleW);

  cv.fillTriangle(tipX,  tipY,  mlX, mlY, mrX, mrY, TFT_RED);    // 北针（红）
  cv.fillTriangle(tailX, tailY, mlX, mlY, mrX, mrY, TFT_WHITE);  // 南针（白）
  cv.fillCircle(cx, cy, 4, TFT_LIGHTGREY);                        // 中心点
}

void FnCompass::_draw() {
  M5Canvas& cv    = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  cv.fillScreen(TFT_BLACK);

  // 标题
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  cv.print("Compass");

  // 指南针圆盘（左侧）
  if (_source != SRC_NONE) {
    _drawRose(cv, COMPASS_CX, COMPASS_CY, COMPASS_R, _heading);
  } else {
    // 静态暗色盘面（无航向数据）
    cv.drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R,     TFT_DARKGREY);
    cv.drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R - 1, TFT_DARKGREY);
    for (int i = 0; i < 16; i++) {
      float a  = i * 22.5f * ((float)M_PI / 180.0f);
      bool isCardinal = (i % 4 == 0);
      int  rIn = isCardinal ? (COMPASS_R - 8) : (COMPASS_R - 5);
      cv.drawLine(COMPASS_CX + (int)(sinf(a) * rIn),
                  COMPASS_CY - (int)(cosf(a) * rIn),
                  COMPASS_CX + (int)(sinf(a) * (COMPASS_R - 2)),
                  COMPASS_CY - (int)(cosf(a) * (COMPASS_R - 2)),
                  TFT_DARKGREY);
    }
    const char* lbl[] = { "N", "E", "S", "W" };
    const float ang[]  = { 0.0f, 90.0f, 180.0f, 270.0f };
    int rLabel = COMPASS_R * 3 / 4;
    cv.setTextSize(1);
    for (int i = 0; i < 4; i++) {
      float a = ang[i] * ((float)M_PI / 180.0f);
      cv.setTextColor(0x2104u);  // 极暗灰
      cv.setCursor(COMPASS_CX + (int)(sinf(a) * rLabel) - 3,
                   COMPASS_CY - (int)(cosf(a) * rLabel) - 4);
      cv.print(lbl[i]);
    }
    cv.setTextSize(2);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(COMPASS_CX - 5, COMPASS_CY - 9);
    cv.print("?");
  }

  // 右侧信息面板
  const int IX = 135;

  // 航向度数（大字）
  cv.setTextSize(3);
  if (_source != SRC_NONE) {
    cv.setTextColor(TFT_WHITE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", _heading);
    cv.setCursor(IX, 16);
    cv.print(buf);
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(IX + 54, 22);
    cv.print("deg");
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(IX, 16);
    cv.print("---");
  }

  // 基数方向
  cv.setTextSize(2);
  if (_source != SRC_NONE) {
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(IX, 52);
    cv.print(cardinalFromHeading(_heading));
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(IX, 52);
    cv.print("--");
  }

  // 速度
  cv.setTextSize(1);
  cv.setCursor(IX, 78);
  if (gps.speedValid()) {
    char sbuf[18];
    snprintf(sbuf, sizeof(sbuf), "%.1f km/h", gps.speedKmph());
    cv.setTextColor(TFT_LIGHTGREY);
    cv.print(sbuf);
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.print("-- km/h");
  }

  // 数据源状态标签
  cv.setCursor(IX, 94);
  switch (_source) {
    case SRC_GPS:
      cv.setTextColor(TFT_GREEN);
      cv.print("GPS COG");
      break;
    case SRC_GYRO:
      cv.setTextColor(TFT_YELLOW);
      cv.print("Gyro drift");
      break;
    case SRC_NONE:
    default:
      cv.setTextColor(0xFD20u);  // 橙色
      cv.print("Need motion");
      cv.setCursor(IX, 106);
      cv.setTextColor(TFT_DARKGREY);
      cv.print("for heading");
      break;
  }

  DisplayManager::instance().commit();
}

// ================================================================
//  菜单图标
// ================================================================

void FnCompass::drawIcon(int x, int y, int size, uint16_t color) {
  drawMenuIcon(DisplayManager::instance().canvas(),
               ICON_COMPASS, x, y, size, color, UI_BG);
}
