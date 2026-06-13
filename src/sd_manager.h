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

class SDManager {
public:
  static SDManager& instance();

  /** 初始化SD卡SPI总线（延迟调用，仅在离线地图功能中使用） */
  bool begin();

  /** SD卡是否已就绪 */
  bool isReady() const { return _ready; }

  /** 加载并解码一个地图瓦片JPEG到画布上 */
  bool loadTile(int z, int x, int y, int screenX, int screenY);

  int maxTileZoom();

  /** 保存上次GPS位置到INI文件 */
  void savePosition(double lat, double lon, int zoom);

  /** 从INI文件加载上次GPS位置 */
  bool loadPosition(double& lat, double& lon, int& zoom);

  /** 获取JPEG解码器引用（供离线地图使用） */
  JPEGDEC& jpegDecoder() { return _jpeg; }

private:
  SDManager() = default;
  SDManager(const SDManager&) = delete;
  SDManager& operator=(const SDManager&) = delete;

  bool _ready = false;
  bool _initAttempted = false;
  int _shotCounter = 0;
  JPEGDEC _jpeg;
};

#endif
