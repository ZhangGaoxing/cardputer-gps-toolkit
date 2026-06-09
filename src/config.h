/**
 * config.h — 全局配置和常量定义
 * 包含所有引脚定义、常量、宏和版本信息
 */
#ifndef CONFIG_H
#define CONFIG_H

#define APP_VERSION "1.0.0"

// ==================================================================
//  GPS 模块引脚（ATGM336H，UART2）
// ==================================================================
#define PIN_GPS_RX   15   // Cardputer ADV Rx ← GPS Tx
#define PIN_GPS_TX   13   // Cardputer ADV Tx → GPS Rx
#define GPS_BAUD      115200

// ==================================================================
//  SD 卡 SPI 引脚（仅离线地图功能使用）
// ==================================================================
#define SD_CS_PIN    12
#define SD_MOSI_PIN  14
#define SD_SCK_PIN   40
#define SD_MISO_PIN  39
#define LORA_CS_PIN  5    // LoRa CS 保持高电平避免 SPI 冲突

// ==================================================================
//  显示屏参数
// ==================================================================
#define SCREEN_W     240
#define SCREEN_H     135
#define STATUSBAR_HEIGHT 14    // 状态栏高度（仅菜单模式使用）
#define CONTENT_TOP  0    // 内容区域起始Y坐标（子功能全屏，菜单模式用 STATUSBAR_HEIGHT 偏移）
#define CONTENT_H    135   // 内容区域高度

// ==================================================================
//  轨迹记录参数
// ==================================================================
#define TRACK_MAX    120   // 轨迹点环形缓冲区大小

// ==================================================================
//  NMEA 监视器参数
// ==================================================================
#define NMEA_BUF_LINES 16
#define NMEA_BUF_WIDTH 84

// ==================================================================
//  GPS 超时与间隔
// ==================================================================
#define GPS_TIMEOUT          1000   // GPS数据超时判定为错误状态(ms)
#define TRACK_RECORD_INTERVAL 2000  // 轨迹记录间隔(ms)

// ==================================================================
//  离线地图参数
// ==================================================================
#define TILE_PX       256
#define ZOOM_MIN      6
#define ZOOM_MAX      18
#define ZOOM_DEFAULT  15
#define MAX_JPEG_BUF  36864
#define PAN_STEP      50
#define DIR_REPEAT_MS 200

// SD 卡地图文件路径
#define PATH_BASE        "/gpsmap"
#define PATH_INI         "/gpsmap/gpsmap.ini"
#define PATH_SHOT_DIR    "/gpsmap/screenshot"

// 矢量地图数据文件（从 map_data.h 的 PROGMEM 数据迁移至 SD 卡）
#define PATH_VECTOR_DIR     "/gpsmap/vector"
#define PATH_COAST_BIN      "/gpsmap/vector/coast.bin"
#define PATH_BORDER_BIN     "/gpsmap/vector/border.bin"
#define PATH_STATE_BIN      "/gpsmap/vector/state.bin"
#define PATH_RIVER_BIN      "/gpsmap/vector/river.bin"
#define PATH_LAKE_BIN       "/gpsmap/vector/lake.bin"
#define PATH_COAST_LOW_BIN  "/gpsmap/vector/coast_low.bin"

// 坐标缩放因子（度 * 100 存储为 int16_t）
#define COORD_SCALE 100

// ==================================================================
//  菜单动画参数
// ==================================================================
// ==================================================================
//  UI 主题色彩（军事绿+黄铜金属风格）
// ==================================================================
#define UI_BG          0x0124   // 深森林绿背景
#define UI_ACTIVE      0x07E0   // 亮荧光绿
#define UI_DIM         0x0300   // 中绿
#define UI_TEXT        0xFFDF   // 象牙白
#define UI_BRASS       0xD4A0   // 黄铜金
#define UI_COPPER      0xC3A0   // 古铜
#define UI_CYAN_GLOW   0x06BF   // 青绿发光
#define UI_DARK_METAL  0x2104   // 暗金属
#define UI_STATUS_BG   0x01A4   // 状态栏深绿
#define UI_TIPS_BG     0x0144   // 底部提示栏背景
#define UI_ACCENT      0x07E0   // 强调色

#define MENU_ICON_SIZE     44   // 选中菜单图标尺寸
#define MENU_SMALL_ICON    22   // 非选中图标尺寸
#define MENU_ITEM_SPACING  110  // 菜单项间距
#define MENU_Y_OFFSET       8   // 非选中项向下偏移
#define MENU_ANIM_DURATION 250  // 动画持续时间(ms)

// ==================================================================
//  屏幕ID枚举 (10个菜单项)
// ==================================================================
enum ScreenID {
  SCR_GPS_DASHBOARD = 0,
  SCR_SIGNAL_BARS,
  SCR_TRIP,
  SCR_GPS_CLOCK,
  SCR_3D_GLOBE,
  SCR_WORLD_MAP,
  SCR_OFFLINE_MAP,
  SCR_WAYPOINT,
  SCR_NMEA_MONITOR,
  SCR_ABOUT,
  SCR_COUNT  // = 10
};

// ==================================================================
//  图标类型枚举
// ==================================================================
enum IconType {
  ICON_DASHBOARD = 0,
  ICON_SIGNAL_BARS,
  ICON_TRIP,
  ICON_CLOCK,
  ICON_3D_GLOBE,
  ICON_WORLD_MAP,
  ICON_OFFLINE_MAP,
  ICON_WAYPOINT,
  ICON_NMEA,
  ICON_ABOUT
};

// ==================================================================
//  GPS 状态枚举
// ==================================================================
enum GPSState { GPS_OFF = 0, GPS_ON, GPS_ERR };

// ==================================================================
//  按键事件结构体
// ==================================================================
struct KeyEvent {
  char key;            // 按键字符或特殊键码
  bool pressed;        // 本次扫描中刚按下
  bool released;       // 本次扫描中刚释放
  bool held;           // 持续按住中
  unsigned long holdDuration;  // 按住时长(ms)
};

#endif // CONFIG_H
