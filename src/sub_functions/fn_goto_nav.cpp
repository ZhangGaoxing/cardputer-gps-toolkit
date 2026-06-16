/**
 * fn_goto_nav.cpp - Go-to waypoint navigation page.
 */
#include "fn_goto_nav.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../navigation_manager.h"
#include "../ui_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void formatDistance(char* out, size_t outSize, float meters) {
  if (!isfinite(meters)) {
    snprintf(out, outSize, "--");
  } else if (meters < 1000.0f) {
    snprintf(out, outSize, "%.0f m", meters);
  } else if (meters < 100000.0f) {
    snprintf(out, outSize, "%.2f km", meters / 1000.0f);
  } else {
    snprintf(out, outSize, "%.1f km", meters / 1000.0f);
  }
}

void formatEta(char* out, size_t outSize, uint32_t seconds) {
  uint32_t hours = seconds / 3600U;
  uint32_t minutes = (seconds % 3600U) / 60U;
  if (hours > 0) {
    snprintf(out, outSize, "%luh%02lu", (unsigned long)hours, (unsigned long)minutes);
  } else {
    snprintf(out, outSize, "%lum", (unsigned long)minutes);
  }
}

void drawLabelValue(M5Canvas& cv, int x, int y, const char* label, const char* value,
                    uint16_t valueColor = TFT_WHITE) {
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(x, y);
  cv.print(label);
  cv.setTextColor(valueColor);
  cv.setCursor(x, y + 10);
  cv.print(value);
}

} // namespace

void FnGotoNav::onEnter() {
}

void FnGotoNav::onUpdate(bool force) {
  (void)force;
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.fillScreen(TFT_BLACK);

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  cv.print("Go-to Nav");

  if (!NavigationManager::instance().isActive()) {
    _drawNoTarget();
  } else {
    _drawActive();
  }
}

bool FnGotoNav::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;
  if (event.key == 'c' || event.key == 'C' ||
      event.key == 'x' || event.key == 'X' ||
      event.key == 's' || event.key == 'S') {
    if (NavigationManager::instance().isActive()) {
      NavigationManager::instance().stop("Canceled");
    }
    return true;
  }
  return false;
}

void FnGotoNav::_drawNoTarget() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(2);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(34, 42);
  cv.print("No Target");

  cv.setTextSize(1);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(20, 70);
  cv.print("Waypoint detail: [g] Go-to");

  const char* err = NavigationManager::instance().lastError();
  if (err[0] != '\0') {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(20, 88);
    cv.print(err);
  }

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[`]Back");
}

void FnGotoNav::_drawActive() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  const NavigationState& st = NavigationManager::instance().state();

  char buf[24];
  char eta[16];
  bool headingValid = isfinite(st.headingDeg);

  cv.setTextSize(1);
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, 15);
  cv.print(st.targetName);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(106, 15);
  cv.printf("ID:%u", st.targetId);

  if (!gps.hasReliableFix() || !st.dataAvailable) {
    cv.setTextSize(2);
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(22, 42);
    cv.print("NAV --");
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(20, 70);
    cv.print(st.lastError[0] ? st.lastError : "No reliable fix");
    cv.setCursor(20, 84);
    cv.printf("Sat:%d HDOP:%.1f", gps.satellitesUsed(), gps.hdop());
    cv.setCursor(4, SCREEN_H - 10);
    cv.print("[c]Cancel  [`]Back");
    return;
  }

  formatDistance(buf, sizeof(buf), st.distanceM);
  cv.setTextSize(3);
  cv.setTextColor(st.arrived ? TFT_GREEN : TFT_CYAN);
  cv.setCursor(4, 32);
  cv.print(buf);

  cv.setTextSize(1);
  cv.setTextColor(st.arrived ? TFT_GREEN : TFT_YELLOW);
  cv.setCursor(154, 34);
  cv.print(st.arrived ? "Arrived" : NavigationManager::relativeText(st.relativeBearingDeg, headingValid));

  if (headingValid) {
    cv.setCursor(154, 46);
    cv.printf("%+.0f deg", st.relativeBearingDeg);
  } else {
    cv.setCursor(154, 46);
    cv.print("Rel --");
  }

  snprintf(buf, sizeof(buf), "%.0f %s", st.bearingDeg, cardinalFromHeading(st.bearingDeg));
  drawLabelValue(cv, 4, 72, "BRG", buf, TFT_WHITE);

  if (headingValid) snprintf(buf, sizeof(buf), "%.0f", st.headingDeg);
  else snprintf(buf, sizeof(buf), "--");
  drawLabelValue(cv, 64, 72, "HDG", buf, headingValid ? TFT_WHITE : TFT_DARKGREY);

  if (st.etaAvailable) formatEta(eta, sizeof(eta), st.etaSeconds);
  else snprintf(eta, sizeof(eta), "--");
  drawLabelValue(cv, 116, 72, "ETA", eta, st.etaAvailable ? TFT_WHITE : TFT_DARKGREY);

  snprintf(buf, sizeof(buf), "Fix %d/%.1f", gps.satellitesUsed(), gps.hdop());
  drawLabelValue(cv, 170, 72, "GPS", buf, gps.hasReliableFix() ? TFT_GREEN : TFT_YELLOW);

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[c]Cancel  [`]Back");
}

void FnGotoNav::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 3;
  cv.drawCircle(cx, cy, r, color);
  cv.drawLine(cx, cy - r, cx, cy - r + 6, color);
  cv.drawLine(cx + r, cy, cx + r - 6, cy, color);
  cv.drawLine(cx, cy + r, cx, cy + r - 6, color);
  cv.drawLine(cx - r, cy, cx - r + 6, cy, color);
  cv.fillTriangle(cx, cy - r + 5, cx - 5, cy + 4, cx + 5, cy + 4, color);
}
