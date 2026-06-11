/**
 * gpx_writer.cpp — GPX轨迹文件写入器实现
 */
#include "gpx_writer.h"
#include "config.h"

GpxWriter& GpxWriter::instance() {
  static GpxWriter gw;
  return gw;
}

bool GpxWriter::begin() {
  if (_sdReady) return true;

  // 禁用LoRa CS避免SPI总线冲突（与SDManager共享SPI）
  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(LORA_CS_PIN, HIGH);

  // 初始化SPI总线
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // 尝试挂载SD卡
  int retries = 3;
  while (retries-- > 0) {
    if (SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ) ||
        SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ_SAFE)) {
      _sdReady = true;

      // 确保存储目录存在
      if (!SD.exists(PATH_BASE)) {
        SD.mkdir(PATH_BASE);
      }
      return true;
    }
    delay(200);
  }

  return false;
}

bool GpxWriter::startRecording(int year, int month, int day,
                                int hour, int minute) {
  if (_recording) {
    stopRecording();
  }

  // 确保SD卡就绪
  if (!_sdReady && !begin()) {
    return false;
  }

  // 生成文件名: Trip_YYYYMMDD_HHmm.gpx
  snprintf(_fileName, sizeof(_fileName),
           "Trip_%04d%02d%02d_%02d%02d.gpx",
           year, month, day, hour, minute);

  snprintf(_filePath, sizeof(_filePath),
           PATH_BASE "/%s", _fileName);

  _file = SD.open(_filePath, FILE_WRITE);
  if (!_file) {
    return false;
  }

  if (!_writeHeader()) {
    _file.close();
    SD.remove(_filePath);
    return false;
  }

  _pointCount = 0;
  _recording = true;
  _recordingStartMs = millis();
  return true;
}

void GpxWriter::stopRecording() {
  if (!_recording) return;

  // 写入闭合标签
  _writeFooter();

  // 安全关闭文件
  if (_file) {
    _file.flush();
    _file.close();
  }

  _recording = false;
  _pointCount = 0;
  _fileName[0] = '\0';
  _filePath[0] = '\0';
  _recordingStartMs = 0;
}

void GpxWriter::appendTrackPoint(float lat, float lon, float altM,
                                  float speedKmph,
                                  int year, int month, int day,
                                  int hour, int minute, float second) {
  if (!_recording || !_file) return;

  char timeBuf[24];
  _formatUtcISO(timeBuf, sizeof(timeBuf),
                year, month, day, hour, minute, second);

  // 构建 <trkpt> 节点
  // 格式: <trkpt lat="xx.xxxxxx" lon="xx.xxxxxx">
  //         <ele>alt</ele>
  //         <time>ISO8601</time>
  //         <speed>speed</speed>
  //       </trkpt>
  _file.printf(
    "<trkpt lat=\"%.6f\" lon=\"%.6f\">\r\n"
    "  <ele>%.1f</ele>\r\n"
    "  <time>%s</time>\r\n"
    "  <speed>%.2f</speed>\r\n"
    "</trkpt>\r\n",
    lat, lon, altM, timeBuf, speedKmph
  );

  // 关键：每次写入后立刻flush到SD卡物理扇区
  // 防止突然断电导致整个轨迹文件损坏
  _file.flush();

  _pointCount++;
}

// ==================================================================
//  内部辅助方法
// ==================================================================

bool GpxWriter::_writeHeader() {
  if (!_file) return false;

  size_t written = _file.print(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
    "<gpx version=\"1.1\" creator=\"CardputerGPS\"\r\n"
    "  xmlns=\"http://www.topografix.com/GPX/1/1\"\r\n"
    "  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\r\n"
    "  xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1"
    " http://www.topografix.com/GPX/1/1/gpx.xsd\">\r\n"
    "  <trk>\r\n"
    "    <name>Trip</name>\r\n"
    "    <trkseg>\r\n"
  );

  _file.flush();
  return written > 0;
}

void GpxWriter::_writeFooter() {
  if (!_file) return;

  _file.print(
    "    </trkseg>\r\n"
    "  </trk>\r\n"
    "</gpx>\r\n"
  );

  // 最后一次flush确保闭合标签写入物理介质
  _file.flush();
}

void GpxWriter::_formatUtcISO(char* buf, int bufSize,
                               int year, int month, int day,
                               int hour, int minute, float second) {
  // ISO 8601格式: YYYY-MM-DDTHH:MM:SS.SSZ
  // 注意：我们不打印小数秒以节省空间，仅打印整数秒
  int secInt = (int)second;
  snprintf(buf, bufSize,
           "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year, month, day, hour, minute, secInt);
}
