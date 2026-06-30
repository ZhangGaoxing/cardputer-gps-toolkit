/**
 * main.cpp - Merged GPS Toolkit entry point
 */
#include <M5Cardputer.h>
#include "config.h"
#include "display_manager.h"
#include "keyboard_manager.h"
#include "menu_system.h"
#include "gps_manager.h"
#include "rtc_manager.h"
#include "battery_manager.h"
#include "imu_manager.h"
#include "trip_tracker.h"
#include "gpx_writer.h"
#include "sd_manager.h"
#include "waypoint_manager.h"
#include "navigation_manager.h"
#include "backtrack_manager.h"
#include "radio_manager.h"
#include "peer_manager.h"

class EscInterceptor : public IKeyListener {
public:
  bool onKeyEvent(const KeyEvent& event) override {
    if (MenuSystem::instance().isInMenu()) return false;

    if (event.pressed && (event.key == '`' || event.key == 0x1B)) {
      SubFunction* fn = MenuSystem::instance().activeFunction();
      if (fn && fn->wantsEsc()) {
        return false;  // 让子功能自己处理 ESC
      }
      MenuSystem::instance().exitFunction();
      return true;
    }
    return false;
  }
};

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);

  DisplaySettingsData displaySettings;
  if (SDManager::instance().begin() && SDManager::instance().loadDisplaySettings(displaySettings)) {
    DisplayManager::instance().setBrightnessLevel(displaySettings.brightnessLevel);
    DisplayManager::instance().setSleepTimeoutIndex(displaySettings.sleepTimeoutIndex);
    GpxWriter::instance().setRecordIntervalIndex(displaySettings.gpxRecordIntervalIndex);
  }

  DisplayManager::instance().begin();

  static EscInterceptor escInterceptor;
  KeyboardManager::instance().setGlobalInterceptor(&escInterceptor);

  GPSManager::instance().begin();
  RTCManager::instance().begin();
  BatteryManager::instance().begin();
  IMUManager::instance().begin();
  TripTracker::instance().begin();
  GpxWriter::instance().begin();
  WaypointManager::instance().begin();
  NavigationManager::instance().begin();
  BacktrackManager::instance().begin();
  PeerManager::instance().begin();
  RadioManager::instance().begin();

  MenuSystem::instance().begin();
  KeyboardManager::instance().setListener(&MenuSystem::instance());

  DisplayManager::instance().clearScreen(TFT_BLACK);
  MenuSystem::instance().draw();
  DisplayManager::instance().commit();
}

void loop() {
  GPSManager::instance().update();

  GPSManager& gps = GPSManager::instance();
  bool hasFreshFix = gps.hasFreshFix();
  bool hasReliableFix = gps.hasReliableFix();

  if (gps.timeValid()) {
    RTCManager::instance().update(
      gps.utcYear(), gps.utcMonth(), gps.utcDay(),
      gps.utcHour(), gps.utcMinute(), gps.utcSecond(),
      gps.latitude(), gps.longitude(),
      hasFreshFix, gps.dateValid()
    );
  }

  BatteryManager::instance().update();
  IMUManager::instance().update();

  // 改进 D+E：低电量时自动开启 GPS 省电模式
  {
    static BatteryAlert lastBatAlert = BatteryAlert::None;
    BatteryAlert batAlert = BatteryManager::instance().alertLevel();
    if (batAlert != lastBatAlert) {
      lastBatAlert = batAlert;
      GPSManager::instance().enablePowerSave(batAlert != BatteryAlert::None);
    }
  }

  NavigationManager::instance().update();

  TripFixQuality tripQuality;
  tripQuality.reliableFix = hasReliableFix;
  tripQuality.hdop = gps.hdop();
  tripQuality.satellitesUsed = gps.satellitesUsed();
  tripQuality.locationAgeMs = gps.fixAgeMs();

  bool tripPointAccepted = TripTracker::instance().update(
    gps.latitude(), gps.longitude(),
    gps.altitude(), gps.speedKmph(),
    tripQuality
  );

  GpxWriter& gw = GpxWriter::instance();
  static bool gpxWasRecording = false;
  static unsigned long lastGpxPointMs = 0;

  if (!gw.isRecording()) {
    gpxWasRecording = false;
    lastGpxPointMs = 0;
  } else {
    unsigned long gpxNow = millis();
    if (!gpxWasRecording) {
      gpxWasRecording = true;
      lastGpxPointMs = 0;
    }

    TripTracker& trk = TripTracker::instance();
    bool segmentStart = tripPointAccepted && trk.lastAcceptedStartedSegment();
    bool intervalDue = lastGpxPointMs == 0 ||
                       gpxNow - lastGpxPointMs >= gw.recordIntervalMs();

    if (hasReliableFix && !trk.lastUpdateRejected() && (segmentStart || intervalDue)) {
      GpxTrackPoint point;
      point.lat = gps.latitude();
      point.lon = gps.longitude();
      point.altM = gps.altitude();
      point.speedKmph = gps.speedKmph();
      point.courseDeg = gps.courseDeg();
      point.hdop = gps.hdop();
      point.pdop = gps.pdop();
      point.vdop = gps.vdop();
      point.satellites = gps.satellitesUsed();
      point.fixMode = gps.fixMode();
      point.fixQuality = gps.fixQuality();
      point.year = gps.utcYear();
      point.month = gps.utcMonth();
      point.day = gps.utcDay();
      point.hour = gps.utcHour();
      point.minute = gps.utcMinute();
      point.second = (float)gps.utcSecond();

      if ((!segmentStart || gw.startNewSegment()) && gw.appendTrackPoint(point)) {
        lastGpxPointMs = gpxNow;
      } else {
        // GpxWriter moves itself to Error and exposes lastError() for the UI.
      }
    }
  }

  // 改进 C：GPX SD 卡抖动后自动恢复（每 5s 尝试一次）
  if (gw.state() == GpxWriterState::Error) {
    static unsigned long lastRecoverAttemptMs = 0;
    unsigned long nowMs = millis();
    if (nowMs - lastRecoverAttemptMs >= 5000UL) {
      lastRecoverAttemptMs = nowMs;
      gw.recoverFromError();
    }
  }

  BacktrackManager::instance().update();
  RadioManager::instance().update();
  {
    RadioPacket packet;
    if (RadioManager::instance().readReceivedPacket(packet)) {
      PeerManager::instance().updateFromPacket(packet);
    }
  }
  PeerManager::instance().update();

  KeyboardManager::instance().scan();
  unsigned long now = millis();
  DisplayManager::instance().updatePowerState(now, KeyboardManager::instance().lastActivityMillis());

  MenuSystem& menu = MenuSystem::instance();

  if (menu.isInMenu()) {
    menu.updateAnimation();
    static unsigned long lastMenuDraw = 0;
    if (now - lastMenuDraw >= 30) {
      lastMenuDraw = now;
      DisplayManager::instance().clearScreen(TFT_BLACK);
      menu.draw();
      DisplayManager::instance().commit();
    }
  } else {
    SubFunction* fn = menu.activeFunction();
    if (fn) {
      static unsigned long lastFnDraw = 0;
      unsigned long interval = fn->updateInterval();

      if (now - lastFnDraw >= interval) {
        lastFnDraw = now;
        if (fn->needsRedraw(now)) {
          DisplayManager::instance().clearScreen(TFT_BLACK);
          fn->onUpdate(false);
          DisplayManager::instance().commit();
        }
      }
    }
  }

  delay(10);
}
