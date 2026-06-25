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
#define SD_SPI_FREQ       10000000U
#define SD_SPI_FREQ_SAFE   4000000U

// ==================================================================
//  LoRa / Mesh position sharing parameters
// ==================================================================
// Defaults follow M5Stack's Cardputer ADV CAP LoRa868/SX1262 demo.
// Confirm the legal regional frequency before transmitting.
#define RADIO_ENABLED                         1
#define RADIO_USE_RADIOLIB                    1
#define RADIO_FREQUENCY                       868.0f
#define RADIO_BANDWIDTH                       500.0f
#define RADIO_SPREADING_FACTOR                7
#define RADIO_CODING_RATE                     5
#define RADIO_TX_POWER                        10
#define RADIO_SYNC_WORD                       0x34
#define RADIO_PREAMBLE_LENGTH                 10
#define RADIO_TCXO_VOLTAGE                    3.0f
#define RADIO_NSS_PIN                         LORA_CS_PIN
#define RADIO_DIO1_PIN                        4
#define RADIO_RST_PIN                         3
#define RADIO_BUSY_PIN                        6
#define RADIO_POSITION_BROADCAST_ENABLED      1
#define RADIO_BROADCAST_INTERVAL_MS           30000UL
#define RADIO_BROADCAST_INTERVAL_LOW_POWER_MS 120000UL
#define RADIO_ALLOW_LAST_KNOWN_BROADCAST      1
#define RADIO_LAST_KNOWN_MAX_AGE_MS           SOS_LAST_FIX_MAX_AGE_MS
#define RADIO_MAX_PACKET_LEN                  180
#define PEER_MAX_COUNT                        12
#define PEER_STALE_TIMEOUT_MS                 120000UL
#define PEER_LOST_TIMEOUT_MS                  600000UL
#define SOS_LORA_REPEAT_COUNT                 3
#define SOS_LORA_REPEAT_INTERVAL_MS           1500UL

// ==================================================================
//  显示屏参数
// ==================================================================
#define SCREEN_W     240
#define SCREEN_H     135
#define STATUSBAR_HEIGHT 14    // 状态栏高度（仅菜单模式使用）
#define CONTENT_TOP  0    // 内容区域起始Y坐标（子功能全屏，菜单模式用 STATUSBAR_HEIGHT 偏移）
#define CONTENT_H    135   // 内容区域高度
#define BRIGHTNESS_LEVEL_COUNT 3
#define SLEEP_TIMEOUT_COUNT   4

// ==================================================================
//  轨迹记录参数
// ==================================================================
#define TRACK_MAX    120   // 轨迹点环形缓冲区大小
#define GPX_TRACK_DIR              PATH_BASE
#define GPX_RECOVERY_ENABLED       1
#define GPX_FLUSH_INTERVAL_MS      5000
#define GPX_FLUSH_EVERY_N_POINTS   3
#define GPX_RECORD_INTERVAL_OPTION_COUNT 6
#define GPX_RECORD_INTERVAL_DEFAULT_INDEX 1

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
#define GPS_FIX_MAX_AGE_MS       3000
#define GPS_NMEA_MAX_AGE_MS      3000
#define GPS_RELIABLE_HDOP_MAX    8.0f
#define GPS_MIN_SATELLITES_USED  4

// TripTracker filtering thresholds
#define TRACK_MIN_SAMPLE_INTERVAL_MS 1000
#define TRACK_MIN_VALID_MOVE_M       2.5f
#define TRACK_MAX_REASONABLE_SPEED_KMPH 160.0f
#define TRACK_MAX_HDOP               6.0f
#define TRACK_MIN_SATELLITES         4
#define TRACK_MAX_LOCATION_AGE_MS    3000
#define TRACK_FIX_GAP_BREAK_MS       5000
#define TRACK_MAX_ALT_JUMP_M         25.0f
#define TRACK_STATIONARY_SPEED_KMPH  1.0f
#define TRACK_STATIONARY_DRIFT_M     6.0f

// ==================================================================
//  离线地图参数
// ==================================================================
#define TILE_PX       256
#define ZOOM_MIN      6
#define ZOOM_MAX      18
#define ZOOM_DEFAULT  15
#define MAP_TILE_CACHE_SIZE          2
#define MAP_NEGATIVE_CACHE_SIZE      4
#define MAP_POSITION_SAVE_INTERVAL_MS 5000UL
#define MAP_MAX_TILE_BYTES           36864
#define MAP_SHOW_MISSING_TILE_DEBUG  0
#define MAX_JPEG_BUF  MAP_MAX_TILE_BYTES
#define PAN_STEP      50
#define DIR_REPEAT_MS 200

// SD 卡地图文件路径
#define PATH_BASE        "/gpstoolkit"
#define PATH_INI         "/gpstoolkit/gpstoolkit.ini"
#define PATH_SHOT_DIR    "/gpstoolkit/screenshot"

// 矢量地图数据文件（从 map_data.h 的 PROGMEM 数据迁移至 SD 卡）
#define PATH_VECTOR_DIR     "/gpstoolkit/vector"
#define PATH_COAST_BIN      "/gpstoolkit/vector/coast.bin"
#define PATH_BORDER_BIN     "/gpstoolkit/vector/border.bin"
#define PATH_STATE_BIN      "/gpstoolkit/vector/state.bin"
#define PATH_RIVER_BIN      "/gpstoolkit/vector/river.bin"
#define PATH_LAKE_BIN       "/gpstoolkit/vector/lake.bin"
#define PATH_CITIES_BIN     "/gpstoolkit/vector/cities.bin"
#define PATH_COAST_LOW_BIN  "/gpstoolkit/vector/coast_low.bin"
#define PATH_BORDER_LOW_BIN "/gpstoolkit/vector/border_low.bin"

#define PATH_COAST_IDX      "/gpstoolkit/vector/coast.idx"
#define PATH_BORDER_IDX     "/gpstoolkit/vector/border.idx"
#define PATH_STATE_IDX      "/gpstoolkit/vector/state.idx"
#define PATH_RIVER_IDX      "/gpstoolkit/vector/river.idx"
#define PATH_LAKE_IDX       "/gpstoolkit/vector/lake.idx"
#define PATH_COAST_LOW_IDX  "/gpstoolkit/vector/coast_low.idx"
#define PATH_BORDER_LOW_IDX "/gpstoolkit/vector/border_low.idx"

// ==================================================================
//  Waypoint 航点参数
// ==================================================================
#define WAYPOINT_MAX_COUNT        100
#define WAYPOINT_NAME_MAX_LEN     16
#define WAYPOINT_NOTE_MAX_LEN     32
#define WAYPOINT_FILE_PATH        "/gpstoolkit/waypoints/waypoints.csv"
#define WAYPOINT_BACKUP_ENABLED   1
#define WAYPOINT_AUTO_SAVE        1

// ==================================================================
//  SOS / Emergency parameters
// ==================================================================
#define SOS_LAST_FIX_WARN_AGE_MS           120000UL
#define SOS_LAST_FIX_MAX_AGE_MS           1800000UL
#define SOS_PAYLOAD_MAX_LEN                    220
#define SOS_SHOW_DMS_COORDS                     1
#define SOS_QUICK_ACCESS_ENABLED                1
#define SOS_NEAREST_WAYPOINT_MAX_DISTANCE_M 5000.0f

// ==================================================================
//  Go-to navigation parameters
// ==================================================================
#define NAV_ARRIVAL_RADIUS_M       20.0f
#define NAV_MIN_SPEED_FOR_ETA_MPS   0.8f
#define NAV_ETA_SMOOTHING_ALPHA     0.25f
#define NAV_OFF_COURSE_ANGLE_DEG   35.0f
#define NAV_TARGET_LINE_ENABLED     1
#define NAV_PERSIST_TARGET_ENABLED  0

// ==================================================================
//  Backtrack navigation parameters
// ==================================================================
#define BACKTRACK_MAX_POINTS             120
#define BACKTRACK_MIN_POINTS_TO_START      4
#define BACKTRACK_MIN_POINT_DISTANCE_M    8.0f
#define BACKTRACK_TARGET_LOOKAHEAD_POINTS 2
#define BACKTRACK_ARRIVAL_RADIUS_M       20.0f
#define BACKTRACK_OFF_ROUTE_DISTANCE_M   50.0f
#define BACKTRACK_RECALC_INTERVAL_MS    500UL
#define BACKTRACK_LINE_DRAW_ENABLED       1

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
//  屏幕ID枚举
// ==================================================================
enum ScreenID {
  SCR_GPS_DASHBOARD = 0,
  SCR_SOS,
  SCR_SIGNAL_BARS,
  SCR_TRIP,
  SCR_BACKTRACK,
  SCR_GPS_CLOCK,
  SCR_3D_GLOBE,
  SCR_WORLD_MAP,
  SCR_OFFLINE_MAP,
  SCR_PEERS,
  SCR_WAYPOINT,
  SCR_GOTO_NAV,
  SCR_NMEA_MONITOR,
  SCR_SETTINGS,
  SCR_ABOUT,
  SCR_COUNT
};

// ==================================================================
//  图标类型枚举
// ==================================================================
enum IconType {
  ICON_DASHBOARD = 0,
  ICON_SOS,
  ICON_SIGNAL_BARS,
  ICON_TRIP,
  ICON_BACKTRACK,
  ICON_CLOCK,
  ICON_3D_GLOBE,
  ICON_WORLD_MAP,
  ICON_OFFLINE_MAP,
  ICON_PEERS,
  ICON_WAYPOINT,
  ICON_GOTO_NAV,
  ICON_NMEA,
  ICON_SETTINGS,
  ICON_ABOUT
};

// ==================================================================
//  GPS 状态枚举
// ==================================================================
enum GPSState {
  GPS_OFF = 0,
  GPS_ON,
  GPS_ERR,
  GPS_SEARCHING,
  GPS_FIX,
  GPS_RELIABLE_FIX
};

// ==================================================================
//  航点来源枚举
// ==================================================================
enum WaypointSource {
  WP_SRC_CURRENT_FIX = 0,   // 从当前位置创建
  WP_SRC_MAP_CENTER,        // 从地图中心创建
  WP_SRC_MANUAL,            // 手动输入坐标
  WP_SRC_IMPORTED           // 从外部导入
};

// ==================================================================
//  航点类型枚举
// ==================================================================
enum WaypointType {
  WP_TYPE_CUSTOM = 0,       // 自定义/通用
  WP_TYPE_CAMP,             // 营地
  WP_TYPE_WATER,            // 水源
  WP_TYPE_DANGER,           // 危险/警告
  WP_TYPE_SUMMIT,           // 山顶
  WP_TYPE_VIEWPOINT,        // 观景点
  WP_TYPE_TRAILHEAD         // 步道起点
};

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
