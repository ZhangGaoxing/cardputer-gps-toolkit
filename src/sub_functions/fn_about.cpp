/**
 * fn_about.cpp — 关于页面
 */
#include "fn_about.h"
#include "../display_manager.h"

void FnAbout::onEnter() {}

void FnAbout::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(1);

  int y = 16;
  int lh = 16;

  // ——— 标题 ———
  cv.setTextColor(TFT_WHITE);
  cv.setTextSize(2);
  cv.setCursor(30, y);
  cv.print("GPS Toolkit");
  cv.setTextSize(1);

  y += 24;

  // ——— 版本 ———
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(60, y);
  cv.print("v0.0.1");
  y += lh + 4;

  // ——— 分隔线 ———
  cv.drawLine(20, y, SCREEN_W - 20, y, UI_DIM);
  y += 8;

  // ——— 作者信息 ———
  cv.setTextColor(UI_BRASS);
  cv.setCursor(20, y);
  cv.print("Author:");
  cv.setTextColor(TFT_CYAN);
  cv.setCursor(80, y);
  cv.print("github.com/ZhangGaoxing");
  y += lh + 6;

  // ——— 致谢 ———
  cv.setTextColor(UI_BRASS);
  cv.setCursor(20, y);
  cv.print("Credits:");
  y += lh;

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(28, y);
  cv.print("lunarc3/CardputerGPSMap");
  y += lh;

  cv.setCursor(28, y);
  cv.print("DevinWatson/Cardputer-Adv-GPS-Info");
  y += lh + 6;
}

void FnAbout::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  // "i" 字母图标 — 信息符号
  int cx = x + size / 2, cy = y + size / 2;
  // Circle
  cv.drawCircle(cx, cy, size / 2 - 2, color);
  // "i" dot
  cv.fillCircle(cx, cy - size / 3, 2, color);
  // "i" body
  cv.fillRect(cx - 1, cy - size / 6, 3, size * 2 / 5, color);
}
