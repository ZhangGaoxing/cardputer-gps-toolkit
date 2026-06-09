/**
 * main.cpp — Merged GPS Toolkit 主入口
 *
 * 硬件：M5Stack Cardputer ADV (ESP32-S3, 240x135, 矩阵键盘)
 * GPS：ATGM336H (UART2: RX15/TX13, 115200bps)
 *
 * 程序流程：
 *   1. setup() 初始化所有管理器
 *   2. loop() 中轮询GPS、IMU、轨迹、键盘
 *   3. 菜单模式下由 MenuSystem 处理绘制和导航
 *   4. 子功能模式下由对应的 SubFunction 处理
 *   5. ESC(`` ` ``)键在任何子功能中都可退回到主菜单
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

// ==================================================================
//  ESC全局退出拦截器
//  在任何子功能中按下 `` ` `` (backtick) 键时返回主菜单
// ==================================================================
class EscInterceptor : public IKeyListener {
public:
  bool onKeyEvent(const KeyEvent& event) override {
    // 只在非菜单模式下拦截ESC/backtick键
    if (MenuSystem::instance().isInMenu()) return false;

    // backtick键(`` ` ``) 或 Esc键码(0x1B)
    if (event.pressed && (event.key == '`' || event.key == 0x1B)) {
      MenuSystem::instance().exitFunction();
      return true;
    }
    return false;
  }
};

// ==================================================================
//  setup() — 系统初始化
// ==================================================================
void setup() {
  // 1. 初始化Cardputer硬件
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);

  // 2. 初始化显示管理器
  DisplayManager::instance().begin();

  // 3. 初始化键盘管理器并设置全局退出拦截器
  static EscInterceptor escInterceptor;
  KeyboardManager::instance().setGlobalInterceptor(&escInterceptor);

  // 4. 初始化GPS管理器
  GPSManager::instance().begin();

  // 5. 初始化RTC管理器
  RTCManager::instance().begin();

  // 6. 初始化电池管理器
  BatteryManager::instance().begin();

  // 7. 初始化IMU管理器
  IMUManager::instance().begin();

  // 8. 初始化轨迹记录器（内存环形缓冲）
  TripTracker::instance().begin();

  // 9. 初始化GPX写入器（SD卡文件持久化，延迟初始化）
  GpxWriter::instance().begin();

  // 10. 初始化菜单系统
  MenuSystem::instance().begin();

  // 11. 将菜单设为当前按键监听者
  KeyboardManager::instance().setListener(&MenuSystem::instance());

  // 12. 首次绘制
  DisplayManager::instance().clearScreen(TFT_BLACK);
  MenuSystem::instance().draw();
  DisplayManager::instance().commit();
}

// ==================================================================
//  loop() — 主循环
// ==================================================================
void loop() {
  // 1. 更新GPS数据（读取串口、解析NMEA）
  GPSManager::instance().update();

  // 2. 同步RTC时间（当GPS有有效时间时）
  GPSManager& gps = GPSManager::instance();
  if (gps.timeValid()) {
    RTCManager::instance().update(
      gps.utcYear(), gps.utcMonth(), gps.utcDay(),
      gps.utcHour(), gps.utcMinute(), gps.utcSecond(),
      gps.latitude(), gps.longitude(),
      gps.hasFix(), gps.dateValid()
    );
  }

  // 3. 更新电池电量（内部有10秒间隔限制）
  BatteryManager::instance().update();

  // 4. 更新IMU传感器数据
  IMUManager::instance().update();

  // 5. 更新轨迹记录（内存环形缓冲）
  TripTracker::instance().update(
    gps.latitude(), gps.longitude(),
    gps.altitude(), gps.speedKmph(),
    gps.hasFix()
  );

  // 6. 更新GPX轨迹录制（SD卡持久化，独立于页面切换）
  {
    GpxWriter& gw = GpxWriter::instance();
    if (gw.isRecording() && gps.hasFix()) {
      static unsigned long lastGpxRecord = 0;
      unsigned long now = millis();
      // 2秒间隔 + 位置变化检查
      if (now - lastGpxRecord >= TRACK_RECORD_INTERVAL) {
        lastGpxRecord = now;
        gw.appendTrackPoint(
          gps.latitude(), gps.longitude(),
          gps.altitude(), gps.speedKmph(),
          gps.utcYear(), gps.utcMonth(), gps.utcDay(),
          gps.utcHour(), gps.utcMinute(),
          (float)gps.utcSecond()
        );
      }
    }
  }

  // 7. 扫描键盘（分发事件到MenuSystem或当前活跃SubFunction）
  KeyboardManager::instance().scan();

  // 8. 渲染
  MenuSystem& menu = MenuSystem::instance();
  unsigned long now = millis();
  if (menu.isInMenu()) {
    // 菜单模式：更新动画并重绘
    menu.updateAnimation();
    // 固定在菜单时重绘（动画需要连续帧）
    static unsigned long lastMenuDraw = 0;
    if (now - lastMenuDraw >= 30) {  // ~30fps 菜单刷新
      lastMenuDraw = now;
      DisplayManager::instance().clearScreen(TFT_BLACK);
      menu.draw();
      DisplayManager::instance().commit();
    }
  } else {
    // 子功能模式：由SubFunction控制绘制节奏
    SubFunction* fn = menu.activeFunction();
    if (fn) {
      static unsigned long lastFnDraw = 0;
      unsigned long interval = fn->updateInterval();

      if (now - lastFnDraw >= interval) {
        lastFnDraw = now;

        // 子功能页面：全屏清屏，不绘制状态栏，子功能占满全屏
        DisplayManager::instance().clearScreen(TFT_BLACK);

        // 绘制子功能内容
        fn->onUpdate(false);
        DisplayManager::instance().commit();
      }
    }
  }

  // 9. 小延迟避免CPU满载
  delay(10);
}
