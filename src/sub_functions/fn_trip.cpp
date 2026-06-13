/**
 * fn_trip.cpp 鈥?琛岀▼锛圫tats + Track + Record 浜斿悎涓€锛孴ab 鍒囨崲锛? * 鍚堝苟鑷?fn_trip_stats.cpp + fn_trip_track.cpp
 */
#include "fn_trip.h"
#include "../display_manager.h"
#include "../trip_tracker.h"
#include "../imu_manager.h"
#include "../gpx_writer.h"
#include "../gps_manager.h"

static void formatDuration(uint32_t ms, char* buf, int bufSize) {
  uint32_t sec = ms / 1000;
  int h = sec / 3600;
  int m = (sec % 3600) / 60;
  int s = sec % 60;
  snprintf(buf, bufSize, "%02d:%02d:%02d", h, m, s);
}

void FnTrip::onEnter() { _subTab = 0; }
void FnTrip::onExit() {
  // 閫€鍑篢rip椤甸潰涓嶅仠姝㈠綍鍒?鈥?褰曞埗鐢盙pxWriter鐙珛绠＄悊
  // 鐢ㄦ埛闇€瑕佸湪Record鏍囩椤垫墜鍔ㄥ仠姝?}
}

void FnTrip::onUpdate(bool force) {
  // Tab header at top (2px from screen edge)
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  const char* tabNames[] = {"Stats", "Record", "Track", "Alt", "Speed"};
  cv.setCursor(4, 2);
  cv.print(tabNames[_subTab]);
  cv.setCursor(SCREEN_W - 40, 2);
  cv.print("[Tab]");

  switch (_subTab) {
    case 0: _drawStats(); break;
    case 1: _drawRecord(); break;
    case 2: _drawTrack();  break;
    case 3: _drawAlt();    break;
    case 4: _drawSpeed();  break;
  }
}

bool FnTrip::onKeyEvent(const KeyEvent& event) {
  if (event.pressed && event.key == 0x09) { _subTab = (_subTab + 1) % 5; return true; }
  // Enter/Space閿湪Record鏍囩椤典腑鎺у埗褰曞埗鍚仠
  if (event.pressed && (event.key == 0x0D || event.key == ' ') && _subTab == 1) {
    GpxWriter& gw = GpxWriter::instance();
    if (gw.isRecording()) {
      gw.stopRecording();
    } else {
      auto& gps = GPSManager::instance();
      if (!gps.dateValid() || !gps.timeValid() || gps.utcYear() < 2020) {
        return true;
      }
      gw.begin();
      gw.startRecording(gps.utcYear(), gps.utcMonth(), gps.utcDay(),
                        gps.utcHour(), gps.utcMinute(), gps.utcSecond());
    }
    return true;
  }
  return false;
}

// ============================================================
//  Tab 0 鈥?琛岀▼缁熻
// ============================================================
void FnTrip::_drawStats() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  IMUManager& imu = IMUManager::instance();
  const TripStats& ts = trk.stats();

  cv.setTextSize(1);
  int y = 14;  // below tab header
  int lh = 14;
  char buf[48];

  if (!ts.hasPrev) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(40, SCREEN_H / 2);
    cv.print("No trip data yet");
    return;
  }

  uint32_t elapsed = millis() - ts.startMillis;
  float avgSpd = (ts.movingMillis > 1000) ? (ts.totalDistKm / (ts.movingMillis / 3600000.0f)) : 0;
  char elBuf[12], mvBuf[12];
  formatDuration(elapsed, elBuf, sizeof(elBuf));
  formatDuration(ts.movingMillis, mvBuf, sizeof(mvBuf));

  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Distance:  %.2f km", ts.totalDistKm);
  cv.print(buf); y += lh;

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Elapsed:   %s", elBuf);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Moving:    %s", mvBuf);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Avg Speed: %.1f km/h", avgSpd);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Speed: %.1f / %.1f km/h", ts.currentSpeedKmph, ts.maxSpeedKmph);
  cv.print(buf); y += lh;

  cv.setTextColor(TFT_CYAN);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt Gain:  +%.0f m", ts.totalAscentM);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt Loss:  -%.0f m", ts.totalDescentM);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt: %.0f-%.0fm  Rej:%lu", ts.minAltM, ts.maxAltM,
           (unsigned long)ts.rejectedPoints);
  cv.print(buf); y += lh;

  if (imu.isAvailable()) {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Max G:     %.2f", imu.maxGForce());
    cv.print(buf);
  }
}

// ============================================================
//  Tab 1 鈥?杞ㄨ抗闈㈠寘灞?// ============================================================
void FnTrip::_drawTrack() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = (int)trk.pointCount();

  const int margin = 4;
  int mapX = margin, mapY = 14, mapW = SCREEN_W - margin * 2, mapH = SCREEN_H - mapY - margin;

  if (cnt < 1) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(50, SCREEN_H / 2); cv.print("No track data yet");
    return;
  }

  TrackPoint cur = trk.pointAt(cnt - 1);
  float cLat = cur.lat;
  float cLon = cur.lon;
  float cosLat = cos(radians(cLat));

  float px[TRACK_MAX], py[TRACK_MAX];
  float maxDist = 1.0;
  for (int i = 0; i < cnt; i++) {
    TrackPoint p = trk.pointAt(i);
    px[i] = (p.lon - cLon) * cosLat * 111320.0;
    py[i] = (p.lat - cLat) * 111320.0;
    float d = max(fabs(px[i]), fabs(py[i]));
    if (d > maxDist) maxDist = d;
  }

  float halfMap = (float)min(mapW, mapH) / 2.0f;
  float scale = halfMap / (maxDist * 1.2);
  int mcx = mapX + mapW / 2, mcy = mapY + mapH / 2;

  cv.drawRect(mapX, mapY, mapW, mapH, TFT_DARKGREY);

  for (int i = 1; i < cnt; i++) {
    int sx1 = mcx + (int)(px[i - 1] * scale);
    int sy1 = mcy - (int)(py[i - 1] * scale);
    int sx2 = mcx + (int)(px[i] * scale);
    int sy2 = mcy - (int)(py[i] * scale);
    if (trk.pointAt(i).segmentStart) continue;
    sx1 = constrain(sx1, mapX + 1, mapX + mapW - 2);
    sy1 = constrain(sy1, mapY + 1, mapY + mapH - 2);
    sx2 = constrain(sx2, mapX + 1, mapX + mapW - 2);
    sy2 = constrain(sy2, mapY + 1, mapY + mapH - 2);
    uint16_t col = (i > cnt / 2) ? TFT_GREEN : TFT_DARKGREY;
    cv.drawLine(sx1, sy1, sx2, sy2, col);
  }
  cv.fillCircle(mcx, mcy, 3, TFT_RED);
}

// ============================================================
//  Tab 2 鈥?娴锋嫈鍓栭潰鍥?// ============================================================
void FnTrip::_drawAlt() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = (int)trk.pointCount();

  int gx = 24, gy = 14, gw = SCREEN_W - gx - 4, gh = SCREEN_H - gy - 14;

  if (cnt < 2) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(40, SCREEN_H / 2); cv.print("Collecting altitude data...");
    return;
  }

  float vals[TRACK_MAX];
  float minV = 99999, maxV = -99999, curV = 0;
  for (int i = 0; i < cnt; i++) { vals[i] = trk.pointAt(i).altM;
    if (vals[i] < minV) minV = vals[i];
    if (vals[i] > maxV) maxV = vals[i]; }
  curV = vals[cnt - 1];
  float range = maxV - minV;
  if (range < 1.0) { range = 1.0; minV -= 0.5; maxV += 0.5; }

  cv.drawLine(gx, gy, gx, gy + gh, TFT_DARKGREY);
  cv.drawLine(gx, gy + gh, gx + gw, gy + gh, TFT_DARKGREY);

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(2, gy); cv.print((int)maxV);
  cv.setCursor(2, gy + gh - 8); cv.print((int)minV);

  for (int i = 1; i < cnt; i++) {
    int x1 = gx + (i - 1) * gw / (cnt - 1);
    int y1 = gy + gh - (int)((vals[i - 1] - minV) / range * gh);
    int x2 = gx + i * gw / (cnt - 1);
    int y2 = gy + gh - (int)((vals[i] - minV) / range * gh);
    cv.drawLine(x1, y1, x2, y2, TFT_CYAN);
  }

  cv.setTextColor(TFT_CYAN);
  cv.setCursor(gx + gw - 70, gy - 4);
  cv.printf("Now:%.0fm", curV);
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(gx + 40, gy - 4);
  cv.print("Alt(m)");

  // X-axis 鐪熷疄鏃堕棿锛?鍒嗛挓闂撮殧
  if (cnt >= 2) {
    cv.setTextColor(TFT_DARKGREY); cv.setTextSize(1);
    unsigned long t0 = trk.pointAt(0).timestamp;
    unsigned long t1 = trk.pointAt(cnt - 1).timestamp;
    unsigned long duration = (t1 > t0) ? (t1 - t0) : 60000;
    unsigned long interval = 120000;
    if (duration < interval * 2) interval = duration / 3;
    if (interval < 30000) interval = 30000;
    for (unsigned long t = (t0 / interval) * interval; t <= t1; t += interval) {
      float frac = (float)(t - t0) / (float)duration;
      int tx = gx + (int)(frac * gw);
      if (tx > gx + gw - 30) tx = gx + gw - 30;
      if (tx < gx) tx = gx;
      char tbuf[6];
      snprintf(tbuf, 6, "%02d:%02d", (int)((t/3600000)%24), (int)((t/60000)%60));
      cv.setCursor(tx, gy + gh + 2);
      cv.print(tbuf);
    }
  }
}

// ============================================================
//  Tab 3 鈥?閫熷害鏇茬嚎鍥?// ============================================================
void FnTrip::_drawSpeed() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = (int)trk.pointCount();

  int gx = 24, gy = 14, gw = SCREEN_W - gx - 4, gh = SCREEN_H - gy - 14;

  if (cnt < 2) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(40, SCREEN_H / 2); cv.print("Collecting speed data...");
    return;
  }

  float vals[TRACK_MAX];
  float minV = 99999, maxV = -99999, curV = 0, sumV = 0;
  for (int i = 0; i < cnt; i++) { vals[i] = trk.pointAt(i).speedKmph;
    if (vals[i] < minV) minV = vals[i];
    if (vals[i] > maxV) maxV = vals[i];
    sumV += vals[i]; }
  curV = vals[cnt - 1];
  float avgV = sumV / cnt;
  if (minV > 0) minV = 0;
  float range = maxV - minV;
  if (range < 1.0) { range = 1.0; maxV = minV + 1.0; }

  cv.drawLine(gx, gy, gx, gy + gh, TFT_DARKGREY);
  cv.drawLine(gx, gy + gh, gx + gw, gy + gh, TFT_DARKGREY);

  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(2, gy); cv.print((int)maxV);
  cv.setCursor(5, gy + gh - 8); cv.print((int)minV);

  for (int i = 1; i < cnt; i++) {
    int x1 = gx + (i - 1) * gw / (cnt - 1);
    int y1 = gy + gh - (int)((vals[i - 1] - minV) / range * gh);
    int x2 = gx + i * gw / (cnt - 1);
    int y2 = gy + gh - (int)((vals[i] - minV) / range * gh);
    cv.drawLine(x1, y1, x2, y2, TFT_YELLOW);
  }

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(gx, gy - 4);
  cv.printf("Spd(km/h)  avg:%.1f", avgV);
  cv.setTextColor(TFT_YELLOW);
  cv.setCursor(gx + gw - 70, gy - 4);
  cv.printf("Now:%.1f", curV);

  // X-axis 鐪熷疄鏃堕棿锛?鍒嗛挓闂撮殧
  if (cnt >= 2) {
    cv.setTextColor(TFT_DARKGREY); cv.setTextSize(1);
    unsigned long t0 = trk.pointAt(0).timestamp;
    unsigned long t1 = trk.pointAt(cnt - 1).timestamp;
    unsigned long duration = (t1 > t0) ? (t1 - t0) : 60000;
    unsigned long interval = 120000;
    if (duration < interval * 2) interval = duration / 3;
    if (interval < 30000) interval = 30000;
    for (unsigned long t = (t0 / interval) * interval; t <= t1; t += interval) {
      float frac = (float)(t - t0) / (float)duration;
      int tx = gx + (int)(frac * gw);
      if (tx > gx + gw - 30) tx = gx + gw - 30;
      if (tx < gx) tx = gx;
      char tbuf[6];
      snprintf(tbuf, 6, "%02d:%02d", (int)((t/3600000)%24), (int)((t/60000)%60));
      cv.setCursor(tx, gy + gh + 2);
      cv.print(tbuf);
    }
  }
}

// ============================================================
//  Tab 4 鈥?GPX 杞ㄨ抗褰曞埗
// ============================================================
void FnTrip::_drawRecord() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GpxWriter& gw = GpxWriter::instance();
  GPSManager& gps = GPSManager::instance();

  cv.setTextSize(1);
  int y = 14;
  const int lh = 12;
  char buf[72];

  bool recording = gw.isRecording();
  bool sdOk = gw.sdReady();
  bool gpsOk = gps.dateValid() && gps.timeValid();
  const char* gpxError = gw.lastError();
  bool showGpxError = gpxError[0] != '\0';
  if (!sdOk &&
      (strcmp(gpxError, "SD init failed") == 0 ||
       strcmp(gpxError, "Cannot create GPX directory") == 0)) {
    showGpxError = false;
  }

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "REC:%s  SD:%s  GPS:%s",
           recording ? "ON" : "OFF",
           sdOk ? "OK" : "NO",
           gpsOk ? "TIME" : "WAIT");
  cv.print(buf); y += lh;

  if (showGpxError) {
    cv.setTextColor(TFT_RED);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "GPX: %.32s", gpxError);
    cv.print(buf); y += lh;
  }

  if (recording) {
    cv.setTextColor(TFT_CYAN);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "File: %.34s", gw.currentFileName());
    cv.print(buf); y += lh;

    unsigned long elapsed = millis() - gw.recordingStartMs();
    int elSec = elapsed / 1000;
    int elMin = elSec / 60;
    int elHr = elMin / 60;
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Pts:%d  Dur:%02d:%02d:%02d",
             gw.pointCount(), elHr, elMin % 60, elSec % 60);
    cv.print(buf); y += lh;

    if (gps.hasReliableFix()) {
      cv.setTextColor(UI_BRASS);
      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "Lat %.6f Lon %.6f",
               gps.latitude(), gps.longitude());
      cv.print(buf); y += lh;

      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "Alt %.0fm Spd %.1f Sat %d",
               gps.altitude(), gps.speedKmph(), gps.satellitesUsed());
      cv.print(buf); y += lh;

      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "HDOP %.1f Fix %s",
               gps.hdop(), gps.fixMode() == 3 ? "3D" : "2D");
      cv.print(buf); y += lh;
    } else {
      cv.setTextColor(TFT_DARKGREY);
      cv.setCursor(4, y);
      cv.print("Waiting for GPS fix...");
      y += lh;

      uint32_t ageMs = gps.fixAgeMs();
      cv.setCursor(4, y);
      if (ageMs == UINT32_MAX) {
        snprintf(buf, sizeof(buf), "HDOP %.1f Sat %d Age --",
                 gps.hdop(), gps.satellitesUsed());
      } else {
        snprintf(buf, sizeof(buf), "HDOP %.1f Sat %d Age %lus",
                 gps.hdop(), gps.satellitesUsed(),
                 (unsigned long)(ageMs / 1000));
      }
      cv.print(buf); y += lh;
    }

    cv.setTextColor(TFT_GREENYELLOW);
    cv.setCursor(4, y);
    cv.print("[Enter] Stop Recording");
  } else {
    if (gpsOk) {
      cv.setTextColor(UI_BRASS);
      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "UTC %04d-%02d-%02d %02d:%02d:%02d",
               gps.utcYear(), gps.utcMonth(), gps.utcDay(),
               gps.utcHour(), gps.utcMinute(), gps.utcSecond());
      cv.print(buf); y += lh;

      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "Next track_%04d%02d%02d_%02d%02d%02d",
               gps.utcYear(), gps.utcMonth(), gps.utcDay(),
               gps.utcHour(), gps.utcMinute(), gps.utcSecond());
      cv.print(buf); y += lh;
    }

    if (!sdOk) {
      cv.setTextColor(TFT_RED);
      cv.setCursor(4, y);
      cv.print("Insert SD card to record");
      y += lh;
    } else if (!gpsOk) {
      cv.setTextColor(TFT_YELLOW);
      cv.setCursor(4, y);
      cv.print("Need valid GPS date/time");
      y += lh;
    }

    cv.setTextColor(sdOk && gpsOk ? TFT_GREEN : TFT_DARKGREY);
    cv.setCursor(4, y);
    cv.print("[Enter] Start Recording");
  }
}

void FnTrip::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int baseY = y + size - 2;
  int bw = size / 4, pad = 3;
  // 3 bars with increasing trend
  cv.fillRect(x + pad, baseY - size * 2/5, bw, size * 2/5, color);
  cv.fillRect(x + pad * 2 + bw, baseY - size * 3/5, bw, size * 3/5, color);
  cv.fillRect(x + pad * 3 + bw * 2, baseY - size * 4/5, bw, size * 4/5, color);
  // Trend arrow
  int ax = x + size - 2, ay = y + 2;
  cv.drawLine(ax - 6, ay + 5, ax, ay, color);
  cv.drawLine(ax, ay, ax + 3, ay, color);
}
