# Cardputer GPS Toolkit

A comprehensive GPS toolkit for M5Stack Cardputer ADV (ESP32-S3), integrating GPS data viewer and offline tile map with a horizontal-scroll menu UI.

## Features (10 Sub-Functions)

| # | Name | Description |
|---|------|-------------|
| 0 | Dashboard | GPS overview: position, speed, heading, satellite summary + mini sky chart |
| 1 | Signal | Satellite SNR bar chart (color-coded by system: GPS/GLONASS/Galileo/BeiDou/QZSS) |
| 2 | Trip | Trip stats + track charts + GPX recording (Tab: Stats/Track/Alt/Speed/Record) |
| 3 | Clock | GPS-synced clock (local time + timezone + DST, supports NA/EU/AU/NZ) |
| 4 | 3D Globe | 3D spinning globe (real-time rendering + satellite orbit visualization, 12°/s auto-rotation) |
| 5 | World Map | Vector world map (Natural Earth coastline data on SD card, 6 zoom levels) |
| 6 | Offline Map | Offline tile map (SD card, OpenStreetMap slippy-map format, zoom 6-18) |
| 7 | Waypoint | Waypoint navigation (compass arrow + bearing + distance, manual target input) |
| 8 | NMEA | NMEA monitor (16-line scrolling raw NMEA sentence display) |
| 9 | About | About page (software info, author, credits) |

## Hardware

| Component | Details |
|-----------|---------|
| MCU | M5Stack Cardputer ADV (ESP32-S3, no PSRAM) |
| GPS Module | ATGM336H (via Cap LoRa-1262 expansion board) |
| GPS Connection | UART2: RX=15, TX=13, 115200 bps |
| SD Card | Required for World Map vector data and offline map tiles (SPI: CS=12, MOSI=14, SCK=40, MISO=39) |
| Display | 240×135 TFT color display |

## Build & Flash

### 1. Install Dependencies

Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI recommended).

### 2. Clone the Project

```bash
git clone https://github.com/ZhangGaoxing/cardputer-gps-toolkit.git
cd cardputer-gps-toolkit
```

### 3. Build

```bash
pio run -e m5cardputer-adv
```

### 4. Upload

Connect Cardputer ADV to your computer via USB-C cable, then:

```bash
pio run -e m5cardputer-adv -t upload
```

### 5. Serial Monitor (Optional)

```bash
pio device monitor -b 115200
```

> **Tip**: If the port is not COM5, modify `upload_port` and `monitor_port` in `platformio.ini`, or override with `--upload-port COMx`.

## Controls

### Main Menu Navigation

| Key | Action |
|-----|--------|
| `,` (comma, left arrow) | Navigate left |
| `/` (slash, right arrow) | Navigate right |
| `Enter` | Open selected function |

* Smooth slide animation between menu items
* Selected item highlighted with green glow; inactive items dimmed
* Each item has a geometric icon

### Global Shortcuts (All Sub-Functions)

| Key | Action |
|-----|--------|
| `` ` `` (backtick, ESC) | **Exit current function, return to main menu** |

### Map Functions

| Key | Function | Action |
|-----|----------|--------|
| `z` | World Map / Offline Map | Zoom in |
| `x` | World Map / Offline Map | Zoom out |
| `;` `.` `,` `/` | Offline Map | Pan (up/down/left/right) |

### Waypoint Function

| Key | Function | Action |
|-----|----------|--------|
| `w` | Waypoint | Enter/exit edit mode |
| `r` | Waypoint | Clear current waypoint |

### Trip Function

| Key | Function | Action |
|-----|----------|--------|
| `Tab` | Trip | Switch sub-tab (Stats → Track → Alt → Speed → Record) |
| `Enter` | Trip (Record tab) | Start/stop GPX track recording |

## World Map Vector Data

World Map keeps all Natural Earth vector resources on the SD card. Copy the contents of `vector_bin/` to `/gpsmap/vector/`:

```
SD Card:
└── gpsmap/
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

The firmware also accepts `/gpsmap/*.bin` and `/vector_bin/*.bin` as fallback locations. `coast.bin` or `border.bin` is enough for the map to open. `cities.bin` enables city dots and labels, and `.idx` files keep zoom/redraw fast by letting the firmware skip off-screen vector segments. If you regenerate any `.bin` file, run `tools/build_vector_indexes.py vector_bin` and copy the updated `.idx` files to the SD card too.

## Offline Map Preparation

The offline map feature uses SD card stored OpenStreetMap tiles. File structure:

```
SD Card:
└── gpsmap/
    ├── gpsmap.ini            ← auto-saved last position
    ├── screenshot/           ← screenshot directory
    │   └── shot_0000.bmp
    └── {z}/                  ← zoom level (6-18)
        └── {x}/
            └── {y}.jpg       ← 256×256 JPEG tile
```

### Generate Map Tiles

Use the Python tools in `GPSMap/maptools/`:

```bash
# Install dependencies
pip install protobuf grpcio-tools numpy Pillow

# Download OpenStreetMap data (.osm.pbf file)
# Generate tiles with the conversion tool
python osm2tile.py input.osm.pbf -z 10-13 -b S,W,N,E
```

Parameters:
- `-z 10-13`: generate tiles for zoom levels 10 through 13
- `-b S,W,N,E`: bounding box (lat South, lon West, lat North, lon East)
- Interactive mode also supported (press Enter to use default zoom 10-12)

> **Note**: Global map requires massive RAM (100+ GB). Convert only the region you need. Zoom 12 is sufficient for daily use.

## Project Structure

```
cardputer-gps-toolkit/
├── platformio.ini              # PlatformIO project config
├── README.md                   # This file (English)
├── README_CN.md                # Chinese version
└── src/
    ├── main.cpp                # Entry point: setup() + loop()
    ├── config.h                # Global config (pins, constants, enums)
    ├── gps_manager.h/.cpp      # GPS manager (UART + NMEA parsing)
    ├── display_manager.h/.cpp  # Display manager (M5Canvas frame buffer)
    ├── keyboard_manager.h/.cpp # Keyboard manager (scan + event dispatch)
    ├── menu_system.h/.cpp      # Menu system (horizontal scroll + animation)
    ├── status_bar.h/.cpp       # Status bar (time + battery)
    ├── rtc_manager.h/.cpp      # RTC manager (GPS time sync + DST)
    ├── battery_manager.h/.cpp  # Battery manager
    ├── imu_manager.h/.cpp      # IMU manager (BMI270)
    ├── sd_manager.h/.cpp       # SD card manager (lazy init)
    ├── trip_tracker.h/.cpp     # Trip tracker + stats (ring buffer)
    ├── gpx_writer.h/.cpp       # GPX file writer (SD card persistence)
    ├── ui_helpers.h/.cpp       # Shared UI drawing utilities
    └── sub_functions/
        ├── sub_function.h      # Sub-function abstract base class
        ├── fn_gps_dashboard.*  # 0: GPS Dashboard
        ├── fn_signal_bars.*    # 1: SNR Bar Chart
        ├── fn_trip.*           # 2: Trip (stats + track + record)
        ├── fn_gps_clock.*      # 3: GPS Clock
        ├── fn_3d_globe.*       # 4: 3D Globe
        ├── fn_world_map.*      # 5: Vector World Map
        ├── fn_offline_map.*    # 6: Offline Tile Map
        ├── fn_waypoint.*       # 7: Waypoint Navigation
        ├── fn_nmea_monitor.*   # 8: NMEA Monitor
        └── fn_about.*          # 9: About Page
```

## Architecture

### Design Patterns

- **Singleton**: All core managers (GPSManager, DisplayManager, KeyboardManager, etc.) are singletons
- **Strategy**: Each sub-function extends the `SubFunction` abstract base class, implementing `onEnter()` / `onUpdate()` / `onKeyEvent()` / `onExit()` lifecycle methods
- **Observer**: `KeyboardManager` dispatches key events to the active listener (menu or sub-function) via the `IKeyListener` interface

### Adding a New Sub-Function

1. Create `src/sub_functions/fn_new_feature.h`
2. Extend `SubFunction` and implement all pure virtual methods
3. Include and register it in the `_items` list in `src/menu_system.cpp`
4. Optionally add entries to `ScreenID` and `IconType` enums in `config.h`

### Memory Strategy

- Single shared `M5Canvas` frame buffer (~65 KB SRAM)
- JPEG decode buffer: `malloc`/`free` per tile (~37 KB peak)
- SD card SPI: lazy initialization for SD-backed map and GPX features
- Satellite list: `std::vector` (dynamic growth)
- World Map vector data stays on SD card; small `.idx` buffers are streamed on demand
- Peak heap usage remains SRAM-friendly for Cardputer ADV without PSRAM

## License

MIT License

## Credits

- Original projects [CardputerGPSMap](https://github.com/lunarc3/CardputerGPSMap) and [Cardputer-Adv-GPS-Info](https://github.com/DevinWatson/Cardputer-Adv-GPS-Info)
- [M5Stack](https://m5stack.com/) — Cardputer ADV hardware platform
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) — NMEA parsing library
- [JPEGDEC](https://github.com/bitbank2/JPEGDEC) — Embedded JPEG decoder
- [Natural Earth](https://www.naturalearthdata.com/) — World map vector data
- [OpenStreetMap](https://www.openstreetmap.org/) — Offline map tile data
