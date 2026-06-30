/**
 * status_bar.cpp — 顶部状态栏实现
 */
#include "status_bar.h"
#include "display_manager.h"
#include "rtc_manager.h"
#include "battery_manager.h"

StatusBar& StatusBar::instance() {
  static StatusBar sb;
  return sb;
}

void StatusBar::draw() {
  M5Canvas& cv = DisplayManager::instance().canvas();

  // 状态栏背景 — 深绿主题
  cv.fillRect(0, 0, SCREEN_W, STATUSBAR_HEIGHT, UI_STATUS_BG);
  cv.drawLine(0, STATUSBAR_HEIGHT - 1, SCREEN_W - 1, STATUSBAR_HEIGHT - 1, UI_DIM);

  // 左上角：当前时间
  _drawTime(4, 2);

  // 右上角：电池电量（右键对齐）
  _drawBattery(SCREEN_W - 4, 2);
}

void StatusBar::_drawTime(int x, int y) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  RTCManager& rtc = RTCManager::instance();

  cv.setTextSize(1);
  cv.setTextColor(UI_TEXT);

  if (rtc.isSynced()) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             rtc.localHour(), rtc.localMinute(), rtc.localSecond());
    cv.setCursor(x, y);
    cv.print(buf);
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(x, y);
    cv.print("--:--:--");
  }
}

void StatusBar::_drawBattery(int rightEdgeX, int y) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  BatteryManager& bat = BatteryManager::instance();

  int pct = bat.percentage();
  BatteryAlert alert = bat.alertLevel();

  // 百分比文字（Critical 加 '!' 前缀警告）
  cv.setTextSize(1);
  char pctBuf[8];
  if (alert == BatteryAlert::Critical10) {
    snprintf(pctBuf, sizeof(pctBuf), "!%d%%", pct);
    cv.setTextColor(TFT_RED);
  } else if (alert == BatteryAlert::Low20) {
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    cv.setTextColor(TFT_YELLOW);
  } else {
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    cv.setTextColor(UI_TEXT);
  }
  int textW = strlen(pctBuf) * 6;

  // 电池图标外框（14x8）
  int bw = 14, bh = 8;
  int battRight = rightEdgeX;
  int battLeft  = battRight - bw - 2;         // 2px 电池正极凸起
  int textX     = battLeft - textW - 3;       // 文字在电池左边，间距 3px

  // 绘制百分比文字（右对齐到电池左侧）
  cv.setCursor(textX, y);
  cv.print(pctBuf);

  // 电池外框（告警时变色）
  uint16_t frameColor = (alert == BatteryAlert::Critical10) ? TFT_RED
                      : (alert == BatteryAlert::Low20)      ? TFT_YELLOW
                      : UI_TEXT;
  cv.drawRect(battLeft, y, bw, bh, frameColor);

  // 电池正极凸起
  cv.fillRect(battLeft + bw, y + 2, 2, 4, frameColor);

  // 电池内部填充（阈值与 BatteryAlert 对齐）
  int fillW = (int)((pct / 100.0f) * (bw - 2));
  if (fillW > 0) {
    uint16_t batColor = (pct > 20) ? UI_ACTIVE
                      : (pct > 10) ? TFT_YELLOW
                      : TFT_RED;
    cv.fillRect(battLeft + 1, y + 1, fillW, bh - 2, batColor);
  }

  // 充电中：在百分比后显示 '+' 号
  if (bat.isCharging()) {
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(battRight + 1, y);
    cv.print("+");
  }
}
