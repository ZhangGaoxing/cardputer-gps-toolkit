/**
 * fn_settings.cpp — 显示设置页面
 */
#include "fn_settings.h"

#include "../display_manager.h"
#include "../gpx_writer.h"
#include "../sd_manager.h"

#include <math.h>

void FnSettings::onEnter() {
  _selectedRow = 0;
  _dirty = true;
  _sdReady = SDManager::instance().begin();
}

void FnSettings::onUpdate(bool force) {
  if (!force && !_dirty) return;

  M5Canvas& cv = DisplayManager::instance().canvas();
  DisplayManager& dm = DisplayManager::instance();

  cv.fillScreen(TFT_BLACK);
  cv.setTextSize(2);
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(58, 10);
  cv.print("Settings");

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(24, 34);
  cv.print("[;]/[.] Select [,][/] Adjust");

  int boxX = 18;
  int boxW = SCREEN_W - 36;
  int rowH = 24;
  int row1Y = 45;
  int row2Y = 73;
  int row3Y = 101;

  uint16_t row1Color = (_selectedRow == 0) ? UI_ACTIVE : UI_DIM;
  uint16_t row2Color = (_selectedRow == 1) ? UI_ACTIVE : UI_DIM;
  uint16_t row3Color = (_selectedRow == 2) ? UI_ACTIVE : UI_DIM;

  cv.drawRoundRect(boxX, row1Y, boxW, rowH, 6, row1Color);
  cv.drawRoundRect(boxX, row2Y, boxW, rowH, 6, row2Color);
  cv.drawRoundRect(boxX, row3Y, boxW, rowH, 6, row3Color);

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(boxX + 10, row1Y + 7);
  cv.print("Brightness");
  cv.setCursor(boxX + 120, row1Y + 7);
  cv.print(dm.brightnessLabel());

  cv.setCursor(boxX + 10, row2Y + 7);
  cv.print("Screen Off");
  cv.setCursor(boxX + 120, row2Y + 7);
  cv.print(dm.sleepTimeoutLabel());

  cv.setCursor(boxX + 10, row3Y + 7);
  cv.print("GPX Interval");
  cv.setCursor(boxX + 120, row3Y + 7);
  cv.print(GpxWriter::instance().recordIntervalLabel());

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(18, 128);
  cv.print(_sdReady ? "Saved to SD ini" : "No SD: session only");

  _dirty = false;
}

bool FnSettings::needsRedraw(unsigned long now) {
  (void)now;
  return _dirty;
}

bool FnSettings::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed && !event.held) return false;

  if (event.pressed && event.key == ';') {
    _moveSelection(-1);
    return true;
  }
  if (event.pressed && event.key == '.') {
    _moveSelection(1);
    return true;
  }
  if (event.key == ',') {
    _adjustCurrent(-1);
    return true;
  }
  if (event.key == '/') {
    _adjustCurrent(1);
    return true;
  }
  return false;
}

void FnSettings::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 3;

  cv.drawCircle(cx, cy, r, color);
  cv.drawCircle(cx, cy, r - 5, color);
  for (int i = 0; i < 8; i++) {
    float ang = i * 0.785398f;
    int x1 = cx + (int)(cosf(ang) * (r - 1));
    int y1 = cy + (int)(sinf(ang) * (r - 1));
    int x2 = cx + (int)(cosf(ang) * (r + 3));
    int y2 = cy + (int)(sinf(ang) * (r + 3));
    cv.drawLine(x1, y1, x2, y2, color);
  }
  cv.fillCircle(cx, cy, 3, color);
}

void FnSettings::_moveSelection(int delta) {
  _selectedRow = (_selectedRow + delta + 3) % 3;
  _dirty = true;
}

void FnSettings::_adjustCurrent(int delta) {
  DisplayManager& dm = DisplayManager::instance();
  GpxWriter& gw = GpxWriter::instance();

  if (_selectedRow == 0) {
    int next = (int)dm.brightnessLevel() + delta;
    if (next < 0) next = BRIGHTNESS_LEVEL_COUNT - 1;
    if (next >= BRIGHTNESS_LEVEL_COUNT) next = 0;
    dm.setBrightnessLevel((uint8_t)next);
  } else if (_selectedRow == 1) {
    int next = (int)dm.sleepTimeoutIndex() + delta;
    if (next < 0) next = SLEEP_TIMEOUT_COUNT - 1;
    if (next >= SLEEP_TIMEOUT_COUNT) next = 0;
    dm.setSleepTimeoutIndex((uint8_t)next);
  } else {
    int next = (int)gw.recordIntervalIndex() + delta;
    if (next < 0) next = GPX_RECORD_INTERVAL_OPTION_COUNT - 1;
    if (next >= GPX_RECORD_INTERVAL_OPTION_COUNT) next = 0;
    gw.setRecordIntervalIndex((uint8_t)next);
  }

  _saveSettings();
  _dirty = true;
}

void FnSettings::_saveSettings() {
  if (!_sdReady) return;
  DisplaySettingsData settings;
  settings.brightnessLevel = DisplayManager::instance().brightnessLevel();
  settings.sleepTimeoutIndex = DisplayManager::instance().sleepTimeoutIndex();
  settings.gpxRecordIntervalIndex = GpxWriter::instance().recordIntervalIndex();
  SDManager::instance().saveDisplaySettings(settings);
}
