/**
 * fn_backtrack.cpp - Backtrack navigation page.
 */
#include "fn_backtrack.h"
#include "../backtrack_manager.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../sd_manager.h"
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

void FnBacktrack::onEnter() {
  _page = PAGE_MAIN;
  _feedback[0] = '\0';
}

void FnBacktrack::onUpdate(bool force) {
  (void)force;
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.fillScreen(TFT_BLACK);

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  cv.print(_page == PAGE_GPX_LIST ? "Load GPX" : "Backtrack");

  if (_page == PAGE_GPX_LIST) {
    _drawGpxList();
    return;
  }

  if (!BacktrackManager::instance().isActive()) {
    _drawInactive();
  } else {
    _drawActive();
  }
}

bool FnBacktrack::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;

  if (_page == PAGE_GPX_LIST) {
    if (event.key == '`' || event.key == 0x1B) {
      _page = PAGE_MAIN;
      return true;
    }
    if (event.key == 'r' || event.key == 'R') {
      _refreshGpxFiles();
      return true;
    }
    if (event.key == ';') {
      if (_gpxCount > 0) {
        if (_selectedIdx > 0) {
          _selectedIdx--;
          if (_selectedIdx < _scrollOffset) _scrollOffset--;
        } else {
          _selectedIdx = _gpxCount - 1;
          _scrollOffset = (_selectedIdx >= GPX_ITEMS_PER_PAGE)
                        ? _selectedIdx - GPX_ITEMS_PER_PAGE + 1
                        : 0;
        }
      }
      return true;
    }
    if (event.key == '.') {
      if (_gpxCount > 0) {
        if (_selectedIdx < _gpxCount - 1) {
          _selectedIdx++;
          if (_selectedIdx >= _scrollOffset + GPX_ITEMS_PER_PAGE) _scrollOffset++;
        } else {
          _selectedIdx = 0;
          _scrollOffset = 0;
        }
      }
      return true;
    }
    if (event.key == 0x0D) {
      _loadSelectedGpx();
      return true;
    }
    return false;
  }

  if (event.key == 'g' || event.key == 'G') {
    _page = PAGE_GPX_LIST;
    _refreshGpxFiles();
    return true;
  }

  if (event.key == 'b' || event.key == 'B') {
    BacktrackManager& bt = BacktrackManager::instance();
    if (!bt.isActive()) {
      if (bt.startFromCurrentTrip(BacktrackTargetMode::ReturnToStart)) {
        _feedback[0] = '\0';
      } else {
        snprintf(_feedback, sizeof(_feedback), "%.47s", bt.lastError());
      }
    }
    return true;
  }

  if (event.key == 'c' || event.key == 'C' ||
      event.key == 'x' || event.key == 'X' ||
      event.key == 's' || event.key == 'S') {
    if (BacktrackManager::instance().isActive()) {
      BacktrackManager::instance().stop("Canceled");
    }
    return true;
  }
  return false;
}

void FnBacktrack::_drawInactive() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(2);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(22, 42);
  cv.print("No Track");

  cv.setTextSize(1);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(18, 70);
  cv.print("[b] Start current Trip");
  cv.setCursor(18, 82);
  cv.print("[g] Load GPX from SD");

  const char* err = BacktrackManager::instance().lastError();
  if (_feedback[0] != '\0') {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(18, 100);
    cv.print(_feedback);
  } else if (err[0] != '\0') {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(18, 100);
    cv.print(err);
  }

}

void FnBacktrack::_drawActive() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  const BacktrackState& st = BacktrackManager::instance().state();

  char buf[24];
  bool headingValid = isfinite(st.headingDeg);

  cv.setTextSize(1);
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, 15);
  cv.print(BacktrackManager::sourceText(st.source));
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(110, 15);
  cv.printf("%u pts", (unsigned)BacktrackManager::instance().pointCount());
  cv.setCursor(166, 15);
  cv.print(BacktrackManager::modeText(st.targetMode));

  if (!gps.hasReliableFix() || !st.dataAvailable) {
    cv.setTextSize(2);
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(22, 42);
    cv.print("BT --");
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(18, 70);
    cv.print(st.lastError[0] ? st.lastError : "No reliable fix");
    cv.setCursor(18, 84);
    cv.printf("Sat:%d HDOP:%.1f", gps.satellitesUsed(), gps.hdop());
    cv.setCursor(4, SCREEN_H - 10);
    cv.print("[c]Cancel");
    return;
  }

  formatDistance(buf, sizeof(buf), st.remainingTrackDistanceM);
  cv.setTextSize(3);
  cv.setTextColor(st.arrived ? TFT_GREEN : (st.offTrack ? TFT_ORANGE : TFT_CYAN));
  cv.setCursor(4, 32);
  cv.print(buf);

  cv.setTextSize(1);
  cv.setTextColor(st.arrived ? TFT_GREEN : (st.offTrack ? TFT_ORANGE : TFT_YELLOW));
  cv.setCursor(154, 34);
  if (st.arrived) {
    cv.print("Arrived");
  } else if (st.offTrack) {
    cv.print("Off Track");
  } else {
    cv.print(BacktrackManager::relativeText(st.relativeBearingDeg, headingValid));
  }

  cv.setCursor(154, 46);
  if (headingValid) {
    cv.printf("%+.0f deg", st.relativeBearingDeg);
  } else {
    cv.print("Rel --");
  }

  snprintf(buf, sizeof(buf), "%.0f %s", st.bearingDeg, cardinalFromHeading(st.bearingDeg));
  drawLabelValue(cv, 4, 72, "BRG", buf, TFT_WHITE);

  formatDistance(buf, sizeof(buf), st.distanceToStartM);
  drawLabelValue(cv, 64, 72, "START", buf, st.arrived ? TFT_GREEN : TFT_WHITE);

  formatDistance(buf, sizeof(buf), st.distanceToTargetM);
  drawLabelValue(cv, 124, 72, "TARGET", buf, TFT_WHITE);

  if (isfinite(st.nearestRouteDistanceM)) {
    formatDistance(buf, sizeof(buf), st.nearestRouteDistanceM);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  drawLabelValue(cv, 184, 72, "ROUTE", buf, st.offTrack ? TFT_ORANGE : TFT_WHITE);

  cv.setTextColor(gps.hasReliableFix() ? TFT_GREEN : TFT_YELLOW);
  cv.setCursor(4, 104);
  cv.printf("GPS %d sat HDOP %.1f  idx:%u",
            gps.satellitesUsed(), gps.hdop(), (unsigned)st.targetIndex);

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[c]Cancel  [g]GPX");
}

void FnBacktrack::_drawGpxList() {
  M5Canvas& cv = DisplayManager::instance().canvas();

  if (!_listLoaded) {
    _refreshGpxFiles();
  }

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(110, 2);
  cv.printf("%d files", _gpxCount);

  if (_feedback[0] != '\0') {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(4, 14);
    cv.print(_feedback);
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, 14);
    cv.print(GPX_TRACK_DIR);
  }

  if (_gpxCount == 0) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(34, 54);
    cv.print("No GPX files found");
    cv.setCursor(22, 72);
    cv.print("Save tracks to SD first");
    cv.setCursor(4, SCREEN_H - 10);
    cv.print("[r]Refresh");
    return;
  }

  int y = 30;
  int rowH = 17;
  int end = _scrollOffset + GPX_ITEMS_PER_PAGE;
  if (end > _gpxCount) end = _gpxCount;

  for (int i = _scrollOffset; i < end; i++) {
    int ry = y + (i - _scrollOffset) * rowH;
    bool selected = (i == _selectedIdx);
    if (selected) {
      cv.fillRect(0, ry, SCREEN_W, rowH - 1, UI_ACTIVE);
      cv.setTextColor(TFT_BLACK);
    } else {
      cv.setTextColor(TFT_WHITE);
    }

    char name[GPX_NAME_MAX];
    strncpy(name, _gpxFiles[i], sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (strlen(name) > 32) {
      name[29] = '.';
      name[30] = '.';
      name[31] = '.';
      name[32] = '\0';
    }
    cv.setCursor(4, ry + 4);
    cv.print(name);
  }

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[;][.]Move [Ent]Load [r]Refresh");
}

void FnBacktrack::_refreshGpxFiles() {
  _gpxCount = 0;
  _selectedIdx = 0;
  _scrollOffset = 0;
  _listLoaded = true;
  _feedback[0] = '\0';

  if (!SDManager::instance().begin()) {
    strncpy(_feedback, "SD not ready", sizeof(_feedback) - 1);
    _feedback[sizeof(_feedback) - 1] = '\0';
    return;
  }

  File dir = SD.open(GPX_TRACK_DIR, FILE_READ);
  if (!dir) {
    strncpy(_feedback, "Cannot open GPX dir", sizeof(_feedback) - 1);
    _feedback[sizeof(_feedback) - 1] = '\0';
    return;
  }

  while (_gpxCount < GPX_LIST_MAX) {
    File entry = dir.openNextFile();
    if (!entry) break;
    bool isDir = entry.isDirectory();
    String entryName = entry.name();
    entry.close();
    if (isDir) continue;

    int slash = entryName.lastIndexOf('/');
    if (slash >= 0) entryName = entryName.substring(slash + 1);
    entryName.trim();
    String lower = entryName;
    lower.toLowerCase();
    if (!lower.endsWith(".gpx")) continue;
    if (lower.endsWith(".gpx.tmp")) continue;

    strncpy(_gpxFiles[_gpxCount], entryName.c_str(), GPX_NAME_MAX - 1);
    _gpxFiles[_gpxCount][GPX_NAME_MAX - 1] = '\0';
    _gpxCount++;
  }
  dir.close();

  if (_gpxCount >= GPX_LIST_MAX) {
    snprintf(_feedback, sizeof(_feedback), "Showing first %d files", GPX_LIST_MAX);
  }
}

void FnBacktrack::_loadSelectedGpx() {
  if (_gpxCount <= 0 || _selectedIdx < 0 || _selectedIdx >= _gpxCount) {
    strncpy(_feedback, "No file selected", sizeof(_feedback) - 1);
    _feedback[sizeof(_feedback) - 1] = '\0';
    return;
  }

  char path[96];
  snprintf(path, sizeof(path), GPX_TRACK_DIR "/%s", _gpxFiles[_selectedIdx]);
  if (BacktrackManager::instance().loadFromGpx(path)) {
    snprintf(_feedback, sizeof(_feedback), "Loaded %.28s", _gpxFiles[_selectedIdx]);
    _page = PAGE_MAIN;
  } else {
    snprintf(_feedback, sizeof(_feedback), "Failed: %.32s",
             BacktrackManager::instance().lastError());
  }
}

void FnBacktrack::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 4;
  cv.drawCircle(cx, cy, r, color);
  cv.drawLine(cx + r - 2, cy, cx + r - 8, cy - 8, color);
  cv.drawLine(cx + r - 8, cy - 8, cx, cy - r + 2, color);
  cv.drawLine(cx, cy - r + 2, cx - r + 8, cy - 8, color);
  cv.drawLine(cx - r + 8, cy - 8, cx - r + 3, cy + 1, color);
  cv.fillTriangle(cx - r + 3, cy + 1, cx - r + 11, cy - 5, cx - r + 9, cy + 6, color);
  cv.drawLine(cx, cy + r - 2, cx, cy - r + 7, color);
  cv.drawLine(cx, cy - r + 7, cx - 5, cy - r + 15, color);
  cv.drawLine(cx, cy - r + 7, cx + 5, cy - r + 15, color);
}
