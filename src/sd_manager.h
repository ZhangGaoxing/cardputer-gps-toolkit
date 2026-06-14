/**
 * sd_manager.h — SD卡管理器 + VectorReader
 */
#ifndef SD_MANAGER_H
#define SD_MANAGER_H
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <JPEGDEC.h>
#include "config.h"

/**
 * VectorReader — 矢量地图数据流式读取器
 *
 * 以 512 字节（1 SD 扇区）内部缓冲区逐 int16_t 读取 SD 卡上的二进制文件。
 * 不将整个文件加载到内存（Cardputer ADV 无 PSRAM）。
 * API 镜像 PROGMEM 的 pgm_read_word() 模式。
 */
class VectorReader {
public:
  bool open(const char* path);
  void close();
  bool isOpen() const { return _isOpen; }
  void rewind();
  bool seek(uint32_t byteOffset);
  int16_t readNext();      // EOF时返回 0x7FFF
  int16_t peekNext();      // EOF时返回 0x7FFF

private:
  File _file;
  bool _isOpen = false;
  bool _eof = false;
  static const size_t BUF_SIZE = 256;  // 256 int16_t = 512 字节
  int16_t _buf[BUF_SIZE];
  size_t _bufPos = 0;
  size_t _bufLen = 0;
  bool _fill();            // 填充缓冲区，EOF 时设 _eof 并返回 false
};

struct VectorSegmentIndexEntry {
  int16_t minLat;
  int16_t maxLat;
  int16_t minLon;
  int16_t maxLon;
  uint32_t dataOffset;
  uint16_t pointCount;
  uint16_t reserved;
};

class VectorIndexReader {
public:
  bool open(const char* path);
  void close();
  bool isOpen() const { return _isOpen; }
  void rewind();
  bool readNext(VectorSegmentIndexEntry& entry);

private:
  File _file;
  bool _isOpen = false;
  static const size_t BUF_SIZE = 8;
  VectorSegmentIndexEntry _buf[BUF_SIZE];
  size_t _bufPos = 0;
  size_t _bufLen = 0;
  bool _fill();
};

enum TileLoadStatus {
  TILE_LOAD_OK = 0,
  TILE_LOAD_NOT_READY,
  TILE_LOAD_NOT_FOUND,
  TILE_LOAD_FILE_OPEN_ERROR,
  TILE_LOAD_FILE_READ_ERROR,
  TILE_LOAD_SIZE_INVALID,
  TILE_LOAD_NO_MEMORY,
  TILE_LOAD_DECODE_OPEN_FAILED,
  TILE_LOAD_DECODE_FAILED
};

struct DisplaySettingsData {
  uint8_t brightnessLevel = 1;
  uint8_t sleepTimeoutIndex = 1;
};

class SDManager {
public:
  static SDManager& instance();

  /** 初始化SD卡SPI总线（延迟调用，仅在离线地图功能中使用） */
  bool begin();

  /** SD卡是否已就绪 */
  bool isReady() const { return _ready; }

  /** 加载并解码一个地图瓦片JPEG到画布上 */
  TileLoadStatus loadTile(int z, int x, int y, int screenX, int screenY);

  int maxTileZoom();

  /** 保存上次GPS位置到INI文件 */
  void savePosition(double lat, double lon, int zoom);

  /** 从INI文件加载上次GPS位置 */
  bool loadPosition(double& lat, double& lon, int& zoom);

  /** 保存显示设置 */
  bool saveDisplaySettings(const DisplaySettingsData& settings);

  /** 加载显示设置 */
  bool loadDisplaySettings(DisplaySettingsData& settings);

  /** 获取JPEG解码器引用（供离线地图使用） */
  JPEGDEC& jpegDecoder() { return _jpeg; }

private:
  struct IniState {
    bool hasPosition = false;
    double lat = 0.0;
    double lon = 0.0;
    int zoom = ZOOM_DEFAULT;
    bool hasDisplaySettings = false;
    DisplaySettingsData displaySettings;
  };

  struct TileCacheEntry {
    bool valid = false;
    uint64_t key = 0;
    size_t size = 0;
    uint32_t lastUsed = 0;
    uint8_t* data = nullptr;
  };

  struct NegativeTileCacheEntry {
    bool valid = false;
    uint64_t key = 0;
    TileLoadStatus status = TILE_LOAD_NOT_FOUND;
    uint32_t lastUsed = 0;
  };

  SDManager() = default;
  SDManager(const SDManager&) = delete;
  SDManager& operator=(const SDManager&) = delete;

  uint64_t _tileKey(int z, int x, int y) const;
  TileCacheEntry* _findTileCache(uint64_t key);
  TileCacheEntry* _selectTileCacheSlot();
  bool _ensureTileBuffer(TileCacheEntry& entry);
  NegativeTileCacheEntry* _findNegativeTileCache(uint64_t key);
  void _rememberNegativeTile(uint64_t key, TileLoadStatus status);
  void _clearNegativeTile(uint64_t key);
  TileLoadStatus _decodeTileBuffer(const uint8_t* data, size_t size, int screenX, int screenY);
  bool _shouldNegativeCache(TileLoadStatus status) const;
  bool _loadIniState(IniState& state);
  bool _saveIniState(const IniState& state);

  bool _ready = false;
  bool _initAttempted = false;
  int _shotCounter = 0;
  JPEGDEC _jpeg;
  uint32_t _tileUseCounter = 0;
  TileCacheEntry _tileCache[MAP_TILE_CACHE_SIZE];
  NegativeTileCacheEntry _negativeTileCache[MAP_NEGATIVE_CACHE_SIZE];
};

#endif
