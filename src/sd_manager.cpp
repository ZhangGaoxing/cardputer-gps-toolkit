/**
 * sd_manager.cpp — SD卡管理器 + VectorReader 实现
 */
#include "sd_manager.h"
#include "display_manager.h"

// ============================================================================
//  VectorReader 实现（流式 SD 读取，512B 扇区缓冲）
// ============================================================================

bool VectorReader::open(const char* path) {
  if (_isOpen) close();
  _file = SD.open(path, FILE_READ);
  _isOpen = (bool)_file;
  _bufPos = 0;
  _bufLen = 0;
  _eof = !_isOpen;
  return _isOpen;
}

void VectorReader::close() {
  if (_file) { _file.close(); _file = File(); }
  _isOpen = false;
  _eof = true;
  _bufPos = 0;
  _bufLen = 0;
}

void VectorReader::rewind() {
  if (_isOpen && _file) {
    _file.seek(0);
  }
  _bufPos = 0;
  _bufLen = 0;
  _eof = false;
}

bool VectorReader::_fill() {
  if (!_isOpen || !_file || _eof) return false;
  size_t toRead = BUF_SIZE * sizeof(int16_t);
  size_t rd = _file.read((uint8_t*)_buf, toRead);
  _bufLen = rd / sizeof(int16_t);
  _bufPos = 0;
  if (_bufLen == 0) {
    _eof = true;
    return false;
  }
  return true;
}

int16_t VectorReader::readNext() {
  if (_bufPos >= _bufLen) {
    if (!_fill()) return 0x7FFF;
  }
  return _buf[_bufPos++];
}

int16_t VectorReader::peekNext() {
  if (_bufPos >= _bufLen) {
    if (!_fill()) return 0x7FFF;
  }
  return _buf[_bufPos];
}

// ============================================================================
//  SDManager 实现
// ============================================================================

// JPEG解码回调 — 字节序修正（JPEGDEC输出大端，TFT期望小端）
static M5Canvas* _jpegTarget = nullptr;

static int _jpegDrawCB(JPEGDRAW* p) {
  if (!_jpegTarget) return 0;
  if (p->x >= SCREEN_W || p->y >= SCREEN_H ||
      p->x + p->iWidth <= 0 || p->y + p->iHeight <= 0)
    return 1;

  uint16_t* px = (uint16_t*)p->pPixels;
  int count = p->iWidth * p->iHeight;
  for (int i = 0; i < count; i++) {
    uint16_t c = px[i];
    px[i] = (c >> 8) | ((c & 0xFF) << 8);
  }

  _jpegTarget->pushImage(p->x, p->y, p->iWidth, p->iHeight, px);
  return 1;
}

SDManager& SDManager::instance() {
  static SDManager sd;
  return sd;
}

bool SDManager::begin() {
  if (_initAttempted) return _ready;
  _initAttempted = true;

  // 禁用LoRa CS避免SPI总线冲突
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(LORA_CS_PIN, HIGH);

  // 初始化SPI总线（使用自定义引脚）
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // 尝试挂载SD卡（最多重试5次）
  for (int retry = 0; retry < 5; retry++) {
    if (SD.begin(SD_CS_PIN)) {
      _ready = true;
      // 初始化截图计数器
      _shotCounter = 0;
      if (SD.exists(PATH_SHOT_DIR)) {
        File dir = SD.open(PATH_SHOT_DIR);
        if (dir) {
          int maxNum = -1;
          while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            String name = entry.name();
            entry.close();
            int lastSlash = name.lastIndexOf('/');
            if (lastSlash >= 0) name = name.substring(lastSlash + 1);
            if (name.startsWith("shot_") && name.endsWith(".bmp")) {
              String numStr = name.substring(5, name.length() - 4);
              int num = numStr.toInt();
              if (num > maxNum) maxNum = num;
            }
          }
          dir.close();
          _shotCounter = (maxNum >= 0) ? maxNum + 1 : 0;
        }
      }
      return true;
    }
    delay(300);
  }

  return false;  // 5次重试全部失败
}

bool SDManager::loadTile(int z, int x, int y, int screenX, int screenY) {
  if (!_ready) return false;

  char path[64];
  snprintf(path, sizeof(path), PATH_BASE "/%d/%d/%d.jpg", z, x, y);

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  size_t sz = f.size();
  if (sz == 0 || sz > MAX_JPEG_BUF) { f.close(); return false; }

  uint8_t* buf = (uint8_t*)malloc(sz);
  if (!buf) { f.close(); return false; }

  f.read(buf, sz);
  f.close();

  _jpegTarget = &DisplayManager::instance().canvas();

  if (_jpeg.openRAM(buf, sz, _jpegDrawCB)) {
    _jpeg.decode(screenX, screenY, 0);
    _jpeg.close();
  }

  free(buf);
  return true;
}

void SDManager::savePosition(double lat, double lon, int zoom) {
  if (!_ready) return;
  SD.mkdir(PATH_BASE);
  File f = SD.open(PATH_INI, FILE_WRITE);
  if (f) {
    f.printf("%.6f,%.6f,%d\n", lat, lon, zoom);
    f.close();
  }
}

bool SDManager::loadPosition(double& lat, double& lon, int& zoom) {
  if (!_ready) return false;
  File f = SD.open(PATH_INI, FILE_READ);
  if (!f) return false;
  String line = f.readStringUntil('\n');
  f.close();
  int c1 = line.indexOf(',');
  int c2 = line.indexOf(',', c1 + 1);
  if (c1 < 0 || c2 < 0) return false;
  lat  = line.substring(0, c1).toFloat();
  lon  = line.substring(c1 + 1, c2).toFloat();
  zoom = line.substring(c2 + 1).toInt();
  if (lat == 0.0 && lon == 0.0) return false;
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
  if (zoom < ZOOM_MIN || zoom > ZOOM_MAX) zoom = ZOOM_DEFAULT;
  return true;
}
