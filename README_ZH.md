# Cardputer GPS Toolkit

适用于 M5Stack Cardputer ADV (ESP32-S3) 的综合 GPS 工具集，集成了 GPS 数据查看器与离线瓦片地图两大功能，提供横向滚动菜单 UI。

## 功能列表（10 个子功能）

| # | 功能名 | 说明 |
|---|--------|------|
| 0 | Dashboard | GPS 综合仪表盘：位置、速度、方向、卫星概览 + 迷你天空图 |
| 1 | Signal | 卫星信噪比柱状图（按系统分色，支持 GPS/GLONASS/Galileo/BeiDou/QZSS） |
| 2 | Trip | 行程（统计 + 轨迹图表 + GPX 录制，Tab 切换 Stats/Track/Alt/Speed/Record） |
| 3 | Clock | GPS 同步时钟（本地时间 + 时区 + 夏令时 DST，支持北美/欧洲/澳洲/新西兰） |
| 4 | 3D Globe | 3D 旋转地球（实时渲染 + 卫星轨道可视化，自动旋转 12°/s） |
| 5 | World Map | 矢量世界地图（SD 卡存储 Natural Earth 海岸线数据，支持 6 级缩放） |
| 6 | Offline Map | 离线瓦片地图（SD 卡存储，OpenStreetMap 标准滑图格式，支持 6-18 级缩放） |
| 7 | Waypoint | 航点管理（列表/详情/从 GPS 或地图中心建点，距离和方位角运行时计算） |
| 8 | NMEA | NMEA 监视器（16 行原始 NMEA 语句滚动显示） |
| 9 | About | 关于页面（软件信息、作者、致谢） |

## 硬件要求

| 组件 | 说明 |
|------|------|
| 主控 | M5Stack Cardputer ADV（ESP32-S3，无 PSRAM） |
| GPS 模块 | ATGM336H（通过 Cap LoRa-1262 扩展板） |
| GPS 连接 | UART2：RX=15, TX=13, 115200 bps |
| SD 卡 | World Map 矢量数据和离线瓦片地图均需要（SPI：CS=12, MOSI=14, SCK=40, MISO=39） |
| 屏幕 | 240×135 TFT 彩色显示屏 |

## 编译与烧录

### 1. 安装依赖

确保已安装 [PlatformIO](https://platformio.org/)（推荐使用 VS Code 扩展或命令行）。

### 2. 克隆/下载项目

```bash
git clone https://github.com/ZhangGaoxing/cardputer-gps-toolkit.git
cd cardputer-gps-toolkit
```

### 3. 编译

```bash
pio run -e m5cardputer-adv
```

### 4. 烧录

用 USB-C 线连接 Cardputer ADV 到电脑，然后：

```bash
pio run -e m5cardputer-adv -t upload
```

### 5. 串口监视（可选）

```bash
pio device monitor -b 115200
```

> **提示**：如果端口不是 COM5，请修改 `platformio.ini` 中的 `upload_port` 和 `monitor_port`，或使用 `--upload-port COMx` 命令行参数覆盖。

## 操作方法

### 主菜单导航

| 按键 | 功能 |
|------|------|
| `,`（逗号，左方向键） | 向左选择菜单项 |
| `/`（斜杠，右方向键） | 向右选择菜单项 |
| `Enter` | 进入当前选中的子功能 |

* 菜单支持平滑滑动动画过渡
* 选中项以绿色高亮边框标识，未选中项为灰色
* 每个菜单项带有几何图形图标

### 通用操作（所有子功能）

| 按键 | 功能 |
|------|------|
| `` ` ``（反引号，ESC） | **退出当前子功能，返回主菜单** |

### 地图功能专用操作

| 按键 | 适用功能 | 功能 |
|------|----------|------|
| `z` | World Map / Offline Map | 放大 |
| `x` | World Map / Offline Map | 缩小 |
| `;` `.` `,` `/` | Offline Map | 平移地图（上下左右） |
| `g` | Offline Map | 重新回到当前 GPS 位置 |
| `w` | Offline Map | 在当前地图中心保存一个航点 |

### 航点功能专用操作

| 按键 | 适用功能 | 功能 |
|------|----------|------|
| `;` `.` | Waypoint 列表页 | 移动选中项 / 循环切换 |
| `Enter` | Waypoint 列表页 | 打开选中航点详情 |
| `n` | Waypoint 列表页 | 进入 GPS 建点页面 |
| `d` | Waypoint 列表页/详情页 | 进入删除确认 |
| `r` | Waypoint 列表页 | 从 SD 卡重新加载航点 |
| `` ` `` | Waypoint 详情/创建/删除页 | 返回航点列表 |
| `Tab` 或 `Space` | Waypoint 创建页 | 编辑航点名称 |
| `Enter` | Waypoint 创建页 | 结束名称编辑 |
| `Backspace` | Waypoint 创建页 | 编辑时删除一个字符 |
| `g` | Waypoint 创建页 | 使用当前 reliable GPS fix 保存航点 |
| `y` / `n` | Waypoint 删除页 | 确认 / 取消删除 |

### 航点使用流程

- 在 Offline Map 页面按 `w`，会以当前地图中心保存航点。这和实时 GPS 位置不同；如果你平移过地图，保存的是地图中心，不是当前位置。
- 在 Waypoint 页面按 `n` 进入 GPS 建点页，再按 `g` 用当前 reliable fix 建点；没有 reliable fix 时会禁止创建。
- 航点文件保存在 SD 卡的 `/gpstoolkit/waypoints/waypoints.csv`，增删改后会自动保存。
- 如果 SD 保存失败，系统会保留旧文件，并通过 `.tmp` / `.bak` 恢复逻辑避免误覆盖；UI 会显示错误，不再把失败误报成成功。
- 距离和方位角始终根据当前 GPS 位置运行时计算，不会写入航点持久化文件。

### 轨迹功能专用操作

| 按键 | 适用功能 | 功能 |
|------|----------|------|
| `Tab` | Trip | 切换子页面（Stats → Track → Alt → Speed → Record） |
| `Enter` | Trip (Record标签) | 开始/停止 GPX 轨迹录制 |

## World Map 矢量数据

World Map 的 Natural Earth 矢量资源全部保存在 SD 卡中。请将 `vector_bin/` 目录内容复制到 SD 卡的 `/gpstoolkit/vector/`：

```
SD卡:
└── gpstoolkit/
    └── vector/
        ├── coast.bin / coast.idx
        ├── border.bin / border.idx
        ├── state.bin / state.idx
        ├── river.bin / river.idx
        ├── lake.bin / lake.idx
        ├── coast_low.bin / coast_low.idx
        ├── border_low.bin / border_low.idx
        └── cities.bin
```

固件也兼容 `/gpstoolkit/*.bin` 和 `/vector_bin/*.bin` 作为备用路径。只要有 `coast.bin` 或 `border.bin`，地图就会打开；`cities.bin` 用于城市点和放大后的地名显示；`.idx` 文件用于按视口跳过屏幕外的矢量线段，显著加快加载和缩放。若重新生成任意 `.bin` 文件，请运行 `tools/build_vector_indexes.py vector_bin`，并把更新后的 `.idx` 一起复制到 SD 卡。

## 离线地图准备

离线地图功能使用 SD 卡存储 OpenStreetMap 瓦片。文件结构如下：

```
SD卡:
└── gpstoolkit/
    ├── gpstoolkit.ini        ← 自动保存上次位置
    ├── screenshot/           ← 截图保存目录
    │   └── shot_0000.bmp
    └── {z}/                  ← 缩放级别 (6-18)
        └── {x}/
            └── {y}.jpg       ← 256×256 JPEG 瓦片
```

### 生成地图瓦片

使用 `tools/` 目录下的 Python 转换工具：

```bash
# 安装依赖
pip install protobuf grpcio-tools numpy Pillow

# 下载 OpenStreetMap 数据（.osm.pbf 文件）
python tools/map_download.py --region asia/china -o tools

# 使用转换工具生成瓦片
python tools/map_convert.py input.osm.pbf -z 10-13 -b S,W,N,E -o gpstoolkit

# 小范围区域可使用 lightweight 引擎
python tools/map_convert.py input.osm.pbf --engine small -z 12-14 -b S,W,N,E -o gpstoolkit

# 两个脚本也支持直接运行后交互输入
python tools/map_download.py
python tools/map_convert.py
```

渲染引擎区别：
- `standard` -> `tools/osm2tile.py`：完整/分阶段渲染器，适合常规区域和较大范围数据，支持 staged/two-pass 处理，区域面和标签处理更完整，推荐作为默认离线地图生成方式。
- `small` -> `tools/osm2tile_small.py`：轻量级 chunk 渲染器，适合小范围快速重生成，依赖和内存压力更低，但风格和要素完整度比 `standard` 更简化。

主要参数说明：
- `input.osm.pbf`：输入的 OSM 数据文件。
- `-z`, `--zoom`：缩放级别或范围，例如 `12` 或 `10-14`。
- `-b`, `--bbox`：边界框，格式为 `south,west,north,east`，例如 `31.1,120.9,31.6,121.8`。
- `-o`, `--output`：输出目录。固件期望目录名为 `gpstoolkit/`，拷贝到 SD 卡后路径应为 `/gpstoolkit/{z}/{x}/{y}.jpg`。
- `--engine standard|small`：选择渲染后端。
- `-l`, `--lang`：地名语言，例如 `en`、`zh`、`ja` 或 `name`。
- `-j`, `--workers`：并行渲染线程数。
- `-q`, `--quality`：JPEG 质量，仅 `small` 引擎使用。
- `--tile-size`：`standard` 引擎的瓦片尺寸。固件要求保持 `256`。
- `-f`, `--format`：`standard` 引擎输出格式。固件要求保持 `JPG`。
- `--single-pass`：强制 `standard` 引擎跳过 staged/two-pass，适合较小数据集或调试。
- `--no-areas`、`--no-ways`、`--no-pois`、`--no-labels`：排查样式或性能问题时可临时关闭某类要素。
- `--dry-run`：只打印最终执行命令，不实际渲染。

参数使用建议：
- `-b` 会同时限制解析和渲染范围，是控制生成时间、磁盘占用和问题定位最重要的参数。
- 如果只需要更新某个城市或区县，建议搭配较小的 `-b` 和较窄的 zoom 范围，例如 `-z 13-14` 或只生成 `-z 14`。
- Cardputer 固件只按 `256x256 JPG` 瓦片设计，其他尺寸或格式不建议用于 SD 卡离线地图。
- 也支持交互模式：直接运行 `python tools/map_convert.py`，按 Enter 接受默认值即可。

> **注意**：全球地图需要大量 RAM（100+ GB），建议只转换所需区域。日常使用 z12 基本满足需求。

## 项目结构

```
cardputer-gps-toolkit/
├── platformio.ini              # PlatformIO 项目配置
├── README.md                   # 本文件
└── src/
    ├── main.cpp                # 主入口 setup() + loop()
    ├── config.h                # 全局配置（引脚、常量、枚举）
    ├── gps_manager.h/.cpp      # GPS 管理器（串口 + NMEA 解析）
    ├── display_manager.h/.cpp  # 显示管理器（M5Canvas 帧缓冲）
    ├── keyboard_manager.h/.cpp # 键盘管理器（扫描 + 事件分发）
    ├── menu_system.h/.cpp      # 菜单系统（横向滚动 + 动画）
    ├── status_bar.h/.cpp       # 状态栏（时间 + 电量）
    ├── rtc_manager.h/.cpp      # RTC 管理器（GPS 时间同步 + DST）
    ├── battery_manager.h/.cpp  # 电池管理器
    ├── imu_manager.h/.cpp      # IMU 管理器（BMI270）
    ├── sd_manager.h/.cpp       # SD 卡管理器（按需加载）
    ├── trip_tracker.h/.cpp     # 轨迹记录 + 行程统计
    ├── gpx_writer.h/.cpp        # GPX 文件写入器
    ├── ui_helpers.h/.cpp       # 共享 UI 绘制函数
    └── sub_functions/
        ├── sub_function.h      # 子功能抽象基类
        ├── fn_gps_dashboard.*  # 0：GPS 仪表盘
        ├── fn_signal_bars.*    # 1：信噪比柱状图
        ├── fn_trip.*           # 2：行程（统计+轨迹+录制）
        ├── fn_gps_clock.*      # 3：GPS 时钟
        ├── fn_3d_globe.*       # 4：3D 旋转地球
        ├── fn_world_map.*      # 5：矢量世界地图
        ├── fn_offline_map.*    # 6：离线瓦片地图
        ├── fn_waypoint.*       # 7：航点导航
        ├── fn_nmea_monitor.*   # 8：NMEA 监视器
        └── fn_about.*          # 9：关于页面
```

## 架构说明

### 设计模式

- **单例模式**：所有核心管理器（GPSManager、DisplayManager、KeyboardManager 等）均为单例
- **策略模式**：每个子功能继承 `SubFunction` 抽象基类，实现自己的 `onEnter()` / `onUpdate()` / `onKeyEvent()` / `onExit()` 生命周期方法
- **观察者模式**：`KeyboardManager` 通过 `IKeyListener` 接口将按键事件分发给当前活跃的监听者（菜单或子功能）

### 添加新子功能

1. 创建 `src/sub_functions/fn_new_feature.h`
2. 继承 `SubFunction` 并实现全部纯虚方法
3. 在 `src/menu_system.cpp` 中 include 并注册到 `_items` 列表
4. 可选：在 `config.h` 的 `ScreenID` 和 `IconType` 枚举中添加新条目

### 内存策略

- 单一共享 `M5Canvas` 帧缓冲（~65KB SRAM）
- JPEG 解码缓冲按需 `malloc`/`free`（~37KB 峰值）
- SD 卡 SPI 按需初始化，用于 SD 地图和 GPX 功能
- 卫星列表使用 `std::vector`（动态增长）
- World Map 矢量数据保留在 SD 卡，仅按需流式读取小块 `.idx` 缓冲
- 峰值堆内存保持适配无 PSRAM 的 Cardputer ADV

## 许可证

MIT License

## 致谢

- 原始项目 [CardputerGPSMap](https://github.com/lunarc3/CardputerGPSMap) 和 [Cardputer-Adv-GPS-Info](https://github.com/DevinWatson/Cardputer-Adv-GPS-Info)
- [M5Stack](https://m5stack.com/) — Cardputer ADV 硬件平台
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) — NMEA 解析库
- [JPEGDEC](https://github.com/bitbank2/JPEGDEC) — 嵌入式 JPEG 解码器
- [Natural Earth](https://www.naturalearthdata.com/) — 世界地图矢量数据
- [OpenStreetMap](https://www.openstreetmap.org/) — 离线地图瓦片数据
