/**
 * fn_trip.cpp — 行程（Stats + Track + Record 五合一，Tab 切换）
 * 合并自 fn_trip_stats.cpp + fn_trip_track.cpp
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
  // 退出Trip页面不停止录制 — 录制由GpxWriter独立管理
  // 用户需要在Record标签页手动停止
}

void FnTrip::onUpdate(bool force) {
  // Tab header at top (2px from screen edge)
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  const char* tabNames[] = {"Stats", "Track", "Alt", "Speed", "Record"};
  cv.setCursor(4, 2);
  cv.print(tabNames[_subTab]);
  cv.setCursor(SCREEN_W - 40, 2);
  cv.print("[Tab]");

  switch (_subTab) {
    case 0: _drawStats(); break;
    case 1: _drawTrack(); break;
    case 2: _drawAlt();   break;
    case 3: _drawSpeed(); break;
    case 4: _drawRecord(); break;
  }
}

bool FnTrip::onKeyEvent(const KeyEvent& event) {
  if (event.pressed && event.key == 0x09) { _subTab = (_subTab + 1) % 5; return true; }
  // Enter/Space键在Record标签页中控制录制启停
  if (event.pressed && (event.key == 0x0D || event.key == ' ') && _subTab == 4) {
    GpxWriter& gw = GpxWriter::instance();
    if (gw.isRecording()) {
      gw.stopRecording();
    } else {
      auto& gps = GPSManager::instance();
      // 必须有有效的GPS日期和时间才能开始录制（文件名依赖UTC时间）
      if (!gps.dateValid() || !gps.timeValid() || gps.utcYear() < 2020) {
        // GPS时间无效 — 静默拒绝，界面已显示"Need valid GPS date/time"
        return true;
      }
      gw.begin();  // 确保SD卡就绪
      gw.startRecording(gps.utcYear(), gps.utcMonth(), gps.utcDay(),
                        gps.utcHour(), gps.utcMinute());
    }
    return true;
  }
  return false;
}

// ============================================================
//  Tab 0 — 行程统计
// ============================================================
void FnTrip::_drawStats() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  IMUManager& imu = IMUManager::instance();
  const TripStats& ts = trk.stats();

  cv.setTextSize(1);
  int y = 14;  // below tab header
  int lh = 14;
  char buf[40];

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
  snprintf(buf, sizeof(buf), "Max Speed: %.1f km/h", ts.maxSpeedKmph);
  cv.print(buf); y += lh;

  cv.setTextColor(TFT_CYAN);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt Gain:  +%.0f m", ts.totalAscentM);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt Loss:  -%.0f m", ts.totalDescentM);
  cv.print(buf); y += lh;

  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Alt Range: %.0f - %.0f m", ts.minAltM, ts.maxAltM);
  cv.print(buf); y += lh;

  if (imu.isAvailable()) {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Max G:     %.2f", imu.maxGForce());
    cv.print(buf);
  }
}

// ============================================================
//  Tab 1 — 轨迹面包屑
// ============================================================
void FnTrip::_drawTrack() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = trk.pointCount();
  const TrackPoint* buf = trk.points();

  const int margin = 4;
  int mapX = margin, mapY = 14, mapW = SCREEN_W - margin * 2, mapH = SCREEN_H - mapY - margin;

  if (cnt < 1) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(50, SCREEN_H / 2); cv.print("No track data yet");
    return;
  }

  int lastRaw = cnt - 1;
  float cLat = buf[lastRaw].lat;
  float cLon = buf[lastRaw].lon;
  float cosLat = cos(radians(cLat));

  float px[TRACK_MAX], py[TRACK_MAX];
  float maxDist = 1.0;
  for (int i = 0; i < cnt; i++) {
    px[i] = (buf[i].lon - cLon) * cosLat * 111320.0;
    py[i] = (buf[i].lat - cLat) * 111320.0;
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
//  Tab 2 — 海拔剖面图
// ============================================================
void FnTrip::_drawAlt() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = trk.pointCount();
  const TrackPoint* buf = trk.points();

  // y轴贴近文本：gx=24, 左侧文本2px处; x轴上方留空给时间标签：gy=14, 底部留空=14px给时间
  int gx = 24, gy = 14, gw = SCREEN_W - gx - 4, gh = SCREEN_H - gy - 14;

  if (cnt < 2) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(40, SCREEN_H / 2); cv.print("Collecting altitude data...");
    return;
  }

  float vals[TRACK_MAX];
  float minV = 99999, maxV = -99999, curV = 0;
  for (int i = 0; i < cnt; i++) { vals[i] = buf[i].altM;
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

  // X-axis 真实时间，2分钟间隔
  if (cnt >= 2) {
    cv.setTextColor(TFT_DARKGREY); cv.setTextSize(1);
    unsigned long t0 = buf[0].timestamp;
    unsigned long t1 = buf[cnt - 1].timestamp;
    unsigned long duration = (t1 > t0) ? (t1 - t0) : 60000;
    // 每2分钟一个标签
    unsigned long interval = 120000;
    if (duration < interval * 2) interval = duration / 3;
    if (interval < 30000) interval = 30000; // 最少30秒间隔
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
//  Tab 3 — 速度曲线图
// ============================================================
void FnTrip::_drawSpeed() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  TripTracker& trk = TripTracker::instance();
  int cnt = trk.pointCount();
  const TrackPoint* buf = trk.points();

  // y轴贴近文本：gx=24, 左侧文本2px处; x轴上方留空给时间标签：gy=14, 底部留空=14px给时间
  int gx = 24, gy = 14, gw = SCREEN_W - gx - 4, gh = SCREEN_H - gy - 14;

  if (cnt < 2) {
    cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(40, SCREEN_H / 2); cv.print("Collecting speed data...");
    return;
  }

  float vals[TRACK_MAX];
  float minV = 99999, maxV = -99999, curV = 0, sumV = 0;
  for (int i = 0; i < cnt; i++) { vals[i] = buf[i].speedKmph;
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

  // X-axis 真实时间，2分钟间隔
  if (cnt >= 2) {
    cv.setTextColor(TFT_DARKGREY); cv.setTextSize(1);
    unsigned long t0 = buf[0].timestamp;
    unsigned long t1 = buf[cnt - 1].timestamp;
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
//  Tab 4 — GPX 轨迹录制
// ============================================================
void FnTrip::_drawRecord() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GpxWriter& gw = GpxWriter::instance();
  GPSManager& gps = GPSManager::instance();

  cv.setTextSize(1);
  int y = 14;
  int lh = 15;
  char buf[64];

  bool recording = gw.isRecording();
  bool sdOk = gw.sdReady();
  bool gpsOk = gps.dateValid() && gps.timeValid();

  // ===== 第一段：状态概览 =====
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Recording: %s", recording ? "ON" : "OFF");
  cv.print(buf); y += lh;

  cv.setTextColor(sdOk ? TFT_GREEN : TFT_RED);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "SD Card:  %s", sdOk ? "Ready" : "No Card");
  cv.print(buf); y += lh;

  cv.setTextColor(gpsOk ? TFT_GREEN : TFT_YELLOW);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "GPS Time: %s", gpsOk ? "Valid" : "Waiting...");
  cv.print(buf); y += lh;

  y += 4;

  // ===== 第二段：录制中显示详情 =====
  if (recording) {
    cv.setTextColor(TFT_CYAN);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "File: %s", gw.currentFileName());
    cv.print(buf); y += lh;

    cv.setTextColor(TFT_WHITE);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Points: %d", gw.pointCount());
    cv.print(buf); y += lh;

    // 录制时长
    unsigned long elapsed = millis() - gw.recordingStartMs();
    int elSec = elapsed / 1000;
    int elMin = elSec / 60;
    int elHr = elMin / 60;
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Duration: %02d:%02d:%02d",
             elHr, elMin % 60, elSec % 60);
    cv.print(buf); y += lh;

    y += 4;

    // ===== 第三段：当前GPS数据预览 =====
    if (gps.hasReliableFix()) {
      cv.setTextColor(UI_BRASS);
      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "Lat: %.6f  Lon: %.6f",
               gps.latitude(), gps.longitude());
      cv.print(buf); y += lh;

      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "Alt: %.0fm  Spd: %.1fkm/h",
               gps.altitude(), gps.speedKmph());
      cv.print(buf); y += lh;
    } else {
      cv.setTextColor(TFT_DARKGREY);
      cv.setCursor(4, y);
      cv.print("Waiting for GPS fix...");
      y += lh;
    }

    y += 4;

    // ===== 第四段：停止提示 =====
    cv.setTextColor(TFT_GREENYELLOW);
    cv.setCursor(4, y);
    cv.print("[Enter] Stop Recording");
  } else {
    // ===== 未录制时的起始提示 =====
    y += 4;

    if (gpsOk) {
      cv.setTextColor(UI_BRASS);
      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "UTC: %04d-%02d-%02d %02d:%02d:%02d",
               gps.utcYear(), gps.utcMonth(), gps.utcDay(),
               gps.utcHour(), gps.utcMinute(), gps.utcSecond());
      cv.print(buf); y += lh;

      cv.setCursor(4, y);
      snprintf(buf, sizeof(buf), "File: Trip_%04d%02d%02d_%02d%02d.gpx",
               gps.utcYear(), gps.utcMonth(), gps.utcDay(),
               gps.utcHour(), gps.utcMinute());
      cv.print(buf); y += lh;
    }

    y += 4;

    cv.setTextColor(TFT_GREEN);
    cv.setCursor(4, y);
    cv.print("[Enter] Start Recording");

    if (!sdOk) {
      y += lh;
      cv.setTextColor(TFT_RED);
      cv.setCursor(4, y);
      cv.print("SD card required!");
    } else if (!gpsOk) {
      y += lh;
      cv.setTextColor(TFT_YELLOW);
      cv.setCursor(4, y);
      cv.print("Need valid GPS date/time");
    }
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
