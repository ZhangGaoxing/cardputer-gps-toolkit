#include "fn_sos.h"

#include "../display_manager.h"
#include "../emergency_info.h"
#include "../geo_format.h"
#include "../ui_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void formatDistance(char* out, size_t outSize, float meters) {
  if (!out || outSize == 0) return;
  if (!isfinite(meters)) {
    snprintf(out, outSize, "--");
  } else if (meters < 1000.0f) {
    snprintf(out, outSize, "%.0fm", meters);
  } else if (meters < 10000.0f) {
    snprintf(out, outSize, "%.2fkm", meters / 1000.0f);
  } else {
    snprintf(out, outSize, "%.1fkm", meters / 1000.0f);
  }
}

void drawLine(M5Canvas& cv, int y, const char* label, const char* value,
              uint16_t valueColor = TFT_WHITE) {
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, y);
  cv.print(label);
  cv.setTextColor(valueColor);
  cv.setCursor(72, y);
  cv.print(value);
}

void drawWrappedText(M5Canvas& cv, int x, int y, int maxChars, int maxRows,
                     const char* text, uint16_t color) {
  if (!text) return;
  cv.setTextSize(1);
  cv.setTextColor(color);

  const char* cursor = text;
  char line[48];
  int row = 0;
  while (*cursor && row < maxRows) {
    int len = 0;
    int lastSpace = -1;
    while (cursor[len] && len < maxChars) {
      if (cursor[len] == ' ') lastSpace = len;
      len++;
    }
    if (cursor[len] && lastSpace > 0) {
      len = lastSpace;
    }
    strncpy(line, cursor, len);
    line[len] = '\0';
    cv.setCursor(x, y + row * 12);
    cv.print(line);
    cursor += len;
    while (*cursor == ' ') cursor++;
    row++;
  }
}

uint16_t statusColor(const EmergencySnapshot& snapshot) {
  if (!snapshot.gpsHasData) return TFT_RED;
  if (snapshot.gpsHasReliableFix) return TFT_GREEN;
  if (snapshot.positionExpired) return TFT_ORANGE;
  if (snapshot.gpsHasFreshFix) return TFT_YELLOW;
  return TFT_ORANGE;
}

} // namespace

void FnSos::onEnter() {
  _page = 0;
}

void FnSos::onUpdate(bool force) {
  (void)force;
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.fillScreen(TFT_BLACK);

  switch (_page) {
    case 0: _drawPageCore(); break;
    case 1: _drawPageNav(); break;
    default: _drawPagePayload(); break;
  }
}

bool FnSos::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;

  if (event.key == 0x09 || event.key == 0x0D || event.key == ' ' || event.key == '.') {
    _page = (_page + 1) % 3;
    return true;
  }
  if (event.key == ';') {
    _page = (_page + 2) % 3;
    return true;
  }
  if (event.key == 'r' || event.key == 'R') {
    return true;
  }
  return false;
}

void FnSos::_drawHeader(const char* title) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(1);
  cv.setTextColor(TFT_RED);
  cv.setCursor(4, 2);
  cv.print(title);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(168, 2);
  cv.printf("%d/3", _page + 1);
}

void FnSos::_drawPageCore() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  EmergencySnapshot snapshot;
  EmergencyInfo::buildSnapshot(snapshot);

  _drawHeader("SOS / Emergency");

  cv.setTextSize(2);
  cv.setTextColor(statusColor(snapshot));
  cv.setCursor(4, 16);
  if (snapshot.positionSource == EMERGENCY_POS_LAST_KNOWN) {
    cv.print("LAST KNOWN");
  } else {
    cv.print(snapshot.gpsStatus);
  }

  cv.setTextSize(1);
  cv.setTextColor(snapshot.positionSource == EMERGENCY_POS_CURRENT ? TFT_CYAN : TFT_YELLOW);
  cv.setCursor(160, 22);
  cv.print(snapshot.positionLabel);

  char latBuf[20];
  char lonBuf[20];
  geoFormatCoordinateDecimal(latBuf, sizeof(latBuf), snapshot.lat, true);
  geoFormatCoordinateDecimal(lonBuf, sizeof(lonBuf), snapshot.lon, false);

  char lineBuf[48];
  snprintf(lineBuf, sizeof(lineBuf), "Lat: %s", snapshot.positionAvailable ? latBuf : "--");
  drawLine(cv, 40, "Coord", lineBuf, snapshot.positionAvailable ? TFT_WHITE : TFT_DARKGREY);
  snprintf(lineBuf, sizeof(lineBuf), "Lon: %s", snapshot.positionAvailable ? lonBuf : "--");
  drawLine(cv, 52, "", lineBuf, snapshot.positionAvailable ? TFT_WHITE : TFT_DARKGREY);

#if SOS_SHOW_DMS_COORDS
  if (snapshot.positionAvailable) {
    char latDms[20];
    char lonDms[20];
    geoFormatCoordinateDms(latDms, sizeof(latDms), snapshot.lat, true);
    geoFormatCoordinateDms(lonDms, sizeof(lonDms), snapshot.lon, false);
    snprintf(lineBuf, sizeof(lineBuf), "%s %s", latDms, lonDms);
    drawLine(cv, 64, "DMS", lineBuf, TFT_CYAN);
  } else {
    drawLine(cv, 64, "DMS", "--", TFT_DARKGREY);
  }
#else
  drawLine(cv, 64, "DMS", "Off", TFT_DARKGREY);
#endif

  if (snapshot.positionAvailable && snapshot.altitudeValid && isfinite(snapshot.altM)) {
    snprintf(lineBuf, sizeof(lineBuf), "%.0fm  Sat %d", snapshot.altM, snapshot.satellites);
  } else {
    snprintf(lineBuf, sizeof(lineBuf), "--  Sat %d", snapshot.satellites);
  }
  drawLine(cv, 76, "Alt", lineBuf, TFT_WHITE);

  // When showing a last-known position, label HDOP as historical to avoid
  // misleading the user into thinking it reflects current signal quality.
  const char* hdopLabel = (snapshot.positionSource == EMERGENCY_POS_LAST_KNOWN)
      ? "LastHDOP" : "HDOP";
  const char* fixQual = snapshot.gpsHasReliableFix ? "Rel"
      : (snapshot.gpsHasFreshFix ? "Fresh" : "NoFix");
  snprintf(lineBuf, sizeof(lineBuf), "%s %.1f  %s  Bat %d%%%s",
           hdopLabel, snapshot.hdop, fixQual,
           snapshot.batteryPct,
           snapshot.batteryCharging ? "+" : "");
  drawLine(cv, 88, "Fix", lineBuf, statusColor(snapshot));
  drawLine(cv, 100, "Time", snapshot.timeText,
           snapshot.timeText[0] != '\0' && strcmp(snapshot.timeText, "Time unavailable") != 0
             ? TFT_WHITE : TFT_DARKGREY);
  drawLine(cv, 112, "Age", snapshot.lastFixAgeText,
           snapshot.positionExpired ? TFT_ORANGE : (snapshot.positionWarnAge ? TFT_YELLOW : TFT_WHITE));

  cv.setTextColor(snapshot.positionExpired ? TFT_ORANGE : TFT_CYAN);
  cv.setCursor(4, 124);
  cv.print(snapshot.credibilityText);
}

void FnSos::_drawPageNav() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  EmergencySnapshot snapshot;
  EmergencyInfo::buildSnapshot(snapshot);
  _drawHeader("SOS / Nav Assist");

  char buf[48];
  char distBuf[20];

  snprintf(buf, sizeof(buf), "%s | %s",
           snapshot.gpsStatus,
           EmergencyInfo::positionSourceText(snapshot.positionSource));
  drawLine(cv, 18, "GPS", buf, statusColor(snapshot));

  if (snapshot.tripStarted) {
    snprintf(buf, sizeof(buf), "Trip %.2fkm", snapshot.tripDistanceKm);
  } else {
    snprintf(buf, sizeof(buf), "Trip not started");
  }
  drawLine(cv, 32, "Trip", buf, snapshot.tripStarted ? TFT_WHITE : TFT_DARKGREY);

  if (snapshot.distanceToStartAvailable) {
    formatDistance(distBuf, sizeof(distBuf), snapshot.distanceToStartM);
    snprintf(buf, sizeof(buf), "%s", distBuf);
  } else {
    snprintf(buf, sizeof(buf), "%s", snapshot.tripStarted ? "--" : "N/A");
  }
  drawLine(cv, 44, "Start", buf, snapshot.distanceToStartAvailable ? TFT_WHITE : TFT_DARKGREY);

  drawLine(cv, 58, "Backtrack", snapshot.backtrack.status,
           snapshot.backtrack.active ? TFT_YELLOW : TFT_DARKGREY);
  if (snapshot.backtrack.active) {
    char startBuf[16];
    char remBuf[16];
    formatDistance(startBuf, sizeof(startBuf), snapshot.backtrack.distanceToStartM);
    formatDistance(remBuf, sizeof(remBuf), snapshot.backtrack.remainingM);
    snprintf(buf, sizeof(buf), "Start %s  Rem %s",
             snapshot.backtrack.distanceToStartAvailable ? startBuf : "--",
             snapshot.backtrack.remainingAvailable ? remBuf : "--");
    drawLine(cv, 70, "", buf, TFT_WHITE);
  } else {
    drawLine(cv, 70, "", "Not active", TFT_DARKGREY);
  }

  if (snapshot.navigation.active) {
    snprintf(buf, sizeof(buf), "%s", snapshot.navigation.targetName[0] ? snapshot.navigation.targetName : "Target");
    drawLine(cv, 84, "Go-to", buf, TFT_CYAN);
    if (snapshot.navigation.distanceAvailable) {
      formatDistance(distBuf, sizeof(distBuf), snapshot.navigation.distanceM);
      snprintf(buf, sizeof(buf), "%s %s", distBuf,
               cardinalFromHeading(snapshot.navigation.bearingDeg));
    } else {
      snprintf(buf, sizeof(buf), "Distance unavailable");
    }
    drawLine(cv, 96, "", buf,
             snapshot.navigation.distanceAvailable ? TFT_WHITE : TFT_DARKGREY);
  } else {
    drawLine(cv, 84, "Go-to", "Inactive", TFT_DARKGREY);
  }

  if (snapshot.nearestWaypoint.available) {
    formatDistance(distBuf, sizeof(distBuf), snapshot.nearestWaypoint.distanceM);
    snprintf(buf, sizeof(buf), "%s %s %s",
             snapshot.nearestWaypoint.name,
             distBuf,
             cardinalFromHeading(snapshot.nearestWaypoint.bearingDeg));
    drawLine(cv, 110, "Nearest", buf, TFT_GREEN);
  } else {
    drawLine(cv, 110, "Nearest", "No nearby waypoint", TFT_DARKGREY);
  }

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 124);
  cv.print("Read-only status. No nav state changes.");
}

void FnSos::_drawPagePayload() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  EmergencySnapshot snapshot;
  EmergencyInfo::buildSnapshot(snapshot);

  char payload[SOS_PAYLOAD_MAX_LEN + 1];
  size_t payloadLen = EmergencyInfo::buildPayload(snapshot, payload, sizeof(payload));

  _drawHeader("SOS Payload");

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 16);
  cv.printf("Len %u/%d", (unsigned)payloadLen, SOS_PAYLOAD_MAX_LEN);
  cv.setCursor(92, 16);
  cv.print(snapshot.positionExpired ? "STALE" : snapshot.gpsStatus);

  cv.drawRect(2, 26, SCREEN_W - 4, 70, UI_DIM);
  drawWrappedText(cv, 6, 32, 37, 5, payload, TFT_WHITE);

  cv.drawLine(2, 100, SCREEN_W - 3, 100, UI_DIM);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 104);
  cv.print("Tip");

  drawWrappedText(cv, 30, 104, 32, 2, snapshot.tipText, TFT_CYAN);

  // cv.drawLine(2, 122, SCREEN_W - 3, 122, UI_DIM);
  // cv.setTextColor(TFT_DARKGREY);
  // cv.setCursor(4, 124);
  // cv.print("[Tab/Ent]Page [r]Refresh [`]Back");
}

void FnSos::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 3;
  cv.drawTriangle(cx, y + 3, x + 3, y + size - 4, x + size - 3, y + size - 4, color);
  cv.drawTriangle(cx, y + 5, x + 5, y + size - 6, x + size - 5, y + size - 6, color);
  cv.fillRect(cx - 2, cy - 10, 4, 12, color);
  cv.fillRect(cx - 2, cy + 5, 4, 4, color);
  cv.drawCircle(cx, cy + 4, r, color);
}