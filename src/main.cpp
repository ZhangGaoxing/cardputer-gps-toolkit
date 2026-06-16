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
  if (gw.isRecording() && hasReliableFix && tripPointAccepted) {
    TripTracker& trk = TripTracker::instance();
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
    if ((!trk.lastAcceptedStartedSegment() || gw.startNewSegment()) &&
        !gw.appendTrackPoint(point)) {
      // GpxWriter moves itself to Error and exposes lastError() for the UI.
    }
  }

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
