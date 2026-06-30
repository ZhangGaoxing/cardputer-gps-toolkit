/**
 * gpx_writer.cpp - GPX track file writer implementation
 */
#include "gpx_writer.h"
#include "config.h"
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

static const char* GPX_FOOTER =
  "    </trkseg>\r\n"
  "  </trk>\r\n"
  "</gpx>\r\n";

static const char* GPX_SEGMENT_BREAK =
  "    </trkseg>\r\n"
  "    <trkseg>\r\n";

static const unsigned long GPX_RECORD_INTERVAL_MS_OPTIONS[] = {
  1000UL, 2000UL, 5000UL, 10000UL, 30000UL, 60000UL
};

static const char* GPX_RECORD_INTERVAL_LABELS[] = {
  "1s", "2s", "5s", "10s", "30s", "60s"
};

static_assert(
  sizeof(GPX_RECORD_INTERVAL_MS_OPTIONS) / sizeof(GPX_RECORD_INTERVAL_MS_OPTIONS[0]) ==
    GPX_RECORD_INTERVAL_OPTION_COUNT,
  "GPX interval option count mismatch");

GpxWriter& GpxWriter::instance() {
  static GpxWriter gw;
  return gw;
}

void GpxWriter::setRecordIntervalIndex(uint8_t index) {
  if (index >= GPX_RECORD_INTERVAL_OPTION_COUNT) {
    index = GPX_RECORD_INTERVAL_DEFAULT_INDEX;
  }
  _recordIntervalIndex = index;
}

unsigned long GpxWriter::recordIntervalMs() const {
  uint8_t index = _recordIntervalIndex;
  if (index >= GPX_RECORD_INTERVAL_OPTION_COUNT) {
    index = GPX_RECORD_INTERVAL_DEFAULT_INDEX;
  }
  return GPX_RECORD_INTERVAL_MS_OPTIONS[index];
}

const char* GpxWriter::recordIntervalLabel() const {
  uint8_t index = _recordIntervalIndex;
  if (index >= GPX_RECORD_INTERVAL_OPTION_COUNT) {
    index = GPX_RECORD_INTERVAL_DEFAULT_INDEX;
  }
  return GPX_RECORD_INTERVAL_LABELS[index];
}

bool GpxWriter::begin() {
  if (_sdReady) return true;

  pinMode(LORA_CS_PIN, OUTPUT);
  digitalWrite(LORA_CS_PIN, HIGH);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  int retries = 3;
  while (retries-- > 0) {
    if (SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ) ||
        SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ_SAFE)) {
      _sdReady = true;
      _clearError();

      if (!SD.exists(GPX_TRACK_DIR) && !SD.mkdir(GPX_TRACK_DIR)) {
        _setError("Cannot create GPX directory");
        return false;
      }

#if GPX_RECOVERY_ENABLED
      _recoverIncompleteFiles();
#endif
      return true;
    }
    delay(200);
  }

  _setError("SD init failed");
  return false;
}

bool GpxWriter::startRecording(int year, int month, int day,
                               int hour, int minute, int second) {
  if (isRecording()) {
    if (!stopRecording()) return false;
  }

  if (!_sdReady && !begin()) {
    return false;
  }

  if (!_makeUniquePaths(year, month, day, hour, minute, second)) {
    _setError("No unique GPX filename");
    return false;
  }

  // Arduino-ESP32 FILE_WRITE truncates existing files. Only open after proving
  // the temp and final paths do not exist, so a recording never overwrites data.
  if (SD.exists(_tmpFilePath) || SD.exists(_filePath)) {
    _setError("GPX file already exists");
    return false;
  }

  _file = SD.open(_tmpFilePath, FILE_WRITE);
  if (!_file) {
    _setError("Cannot create GPX temp file");
    return false;
  }

  _pointCount = 0;
  _pointsSinceFlush = 0;
  _bytesWritten = 0;

  if (!_writeHeader()) {
    _file.close();
    SD.remove(_tmpFilePath);
    _setError("Cannot write GPX header");
    return false;
  }

  _recordingStartMs = millis();
  _lastFlushMs = _recordingStartMs;
  _state = GpxWriterState::Recording;
  _clearError();
  return true;
}

bool GpxWriter::stopRecording() {
  if (!isRecording()) return true;

  bool ok = _writeFooter();
  ok = _flushIfDue(true) && ok;

  if (_file) {
    _file.close();
  }

  if (ok) {
    if (SD.exists(_filePath)) {
      ok = false;
      _setError("Final GPX file exists");
    } else if (!SD.rename(_tmpFilePath, _filePath)) {
      ok = false;
      _setError("Cannot finalize GPX file");
    }
  }

  _pointCount = 0;
  _pointsSinceFlush = 0;
  _bytesWritten = 0;
  _recordingStartMs = 0;
  _lastFlushMs = 0;
  _file = File();

  if (ok) {
    _state = GpxWriterState::Closed;
    _tmpFilePath[0] = '\0';
    return true;
  }

  _state = GpxWriterState::Error;
  return false;
}

bool GpxWriter::startNewSegment() {
  if (!isRecording() || !_file) return false;
  if (_pointCount <= 0) return true;

  if (!_writeRaw(GPX_SEGMENT_BREAK) || !_flushIfDue(true)) {
    _writeFailCount++;
    if (_writeFailCount >= 3) {
      if (_file) _file.close();
      _state = GpxWriterState::Error;
      _sdReady = false;
      _setError("GPX segment failed");
    } else {
      if (_file) _file.flush();
      _setError("GPX segment retry");
    }
    return false;
  }
  _writeFailCount = 0;
  return true;
}

// SD 卡抗拾后自动恢复：重新打开临时文件并写入分段符，保持 GPX 合法，然后恢复 Recording 状态
bool GpxWriter::recoverFromError() {
  if (_state != GpxWriterState::Error) return true;
  if (_tmpFilePath[0] == '\0') {
    _setError("No active recording to recover");
    return false;
  }

  // 重新初始化 SD
  if (!SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ) &&
      !SD.begin(SD_CS_PIN, SPI, SD_SPI_FREQ_SAFE)) {
    _setError("SD recover: init failed");
    return false;
  }
  _sdReady = true;

  // 以追加模式重新打开临时文件
  _file = SD.open(_tmpFilePath, FILE_APPEND);
  if (!_file) {
    _setError("SD recover: reopen failed");
    return false;
  }

  // 写入分段符以标记间隙（保持 GPX 合法性）
  if (!_writeRaw(GPX_SEGMENT_BREAK)) {
    if (_file) _file.close();
    _setError("SD recover: segment write failed");
    return false;
  }
  _file.flush();

  _state = GpxWriterState::Recording;
  _writeFailCount = 0;
  _clearError();
  return true;
}

bool GpxWriter::appendTrackPoint(const GpxTrackPoint& point) {
  if (!isRecording() || !_file) return false;
  if (!isfinite(point.lat) || !isfinite(point.lon) ||
      !isfinite(point.altM) || !isfinite(point.speedKmph)) {
    _setError("Invalid GPX point");
    _state = GpxWriterState::Error;
    return false;
  }

  char timeBuf[24];
  _formatUtcISO(timeBuf, sizeof(timeBuf),
                point.year, point.month, point.day,
                point.hour, point.minute, point.second);

  bool ok = true;
  ok = _writeFormat("    <trkpt lat=\"%.6f\" lon=\"%.6f\">\r\n",
                    point.lat, point.lon) && ok;
  ok = _writeFormat("      <ele>%.1f</ele>\r\n", point.altM) && ok;
  ok = _writeFormat("      <time>%s</time>\r\n", timeBuf) && ok;
  if (point.fixMode == 2 || point.fixMode == 3) {
    ok = _writeFormat("      <fix>%s</fix>\r\n", _fixText(point.fixMode)) && ok;
  }
  if (point.satellites > 0) {
    ok = _writeFormat("      <sat>%d</sat>\r\n", point.satellites) && ok;
  }
  if (isfinite(point.hdop) && point.hdop < 99.0f) {
    ok = _writeFormat("      <hdop>%.1f</hdop>\r\n", point.hdop) && ok;
  }
  if (isfinite(point.vdop) && point.vdop < 99.0f) {
    ok = _writeFormat("      <vdop>%.1f</vdop>\r\n", point.vdop) && ok;
  }
  if (isfinite(point.pdop) && point.pdop < 99.0f) {
    ok = _writeFormat("      <pdop>%.1f</pdop>\r\n", point.pdop) && ok;
  }

  // GPX 1.1 does not define speed/course as direct trkpt children. Keep them
  // in a private extension so validators still accept the file.
  ok = _writeRaw("      <extensions>\r\n") && ok;
  ok = _writeFormat("        <cardputer:speedKmph>%.2f</cardputer:speedKmph>\r\n",
                    point.speedKmph) && ok;
  ok = _writeFormat("        <cardputer:speedMps>%.2f</cardputer:speedMps>\r\n",
                    point.speedKmph / 3.6f) && ok;
  if (isfinite(point.courseDeg)) {
    ok = _writeFormat("        <cardputer:course>%.1f</cardputer:course>\r\n",
                      point.courseDeg) && ok;
  }
  if (point.fixQuality > 0) {
    ok = _writeFormat("        <cardputer:fixQuality>%d</cardputer:fixQuality>\r\n",
                      point.fixQuality) && ok;
  }
  ok = _writeRaw("      </extensions>\r\n") && ok;
  ok = _writeRaw("    </trkpt>\r\n") && ok;

  _pointsSinceFlush++;

  if (!ok || !_flushIfDue(false)) {
    _writeFailCount++;
    if (_writeFailCount >= 3) {
      if (_file) _file.close();
      _state = GpxWriterState::Error;
      _sdReady = false;
      _setError("GPX write failed");
    } else {
      // 临时写入失败，尝试 flush 恢复；调用方可在下一帧重试
      if (_file) _file.flush();
      _setError("GPX write retry");
    }
    return false;
  }

  _writeFailCount = 0;
  _pointCount++;
  return true;
}

bool GpxWriter::_writeHeader() {
  if (!_file) return false;

  return _writeRaw(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
    "<gpx version=\"1.1\" creator=\"CardputerGPS\"\r\n"
    "  xmlns=\"http://www.topografix.com/GPX/1/1\"\r\n"
    "  xmlns:cardputer=\"https://github.com/cardputer-gps-toolkit/gpx/extensions\"\r\n"
    "  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\r\n"
    "  xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1"
    " http://www.topografix.com/GPX/1/1/gpx.xsd\">\r\n"
    "  <trk>\r\n"
    "    <name>Trip</name>\r\n"
    "    <trkseg>\r\n"
  ) && _flushIfDue(true);
}

bool GpxWriter::_writeFooter() {
  if (!_file) return false;
  return _writeRaw(GPX_FOOTER);
}

bool GpxWriter::_writeRaw(const char* text) {
  if (!_file || !text) return false;
  size_t len = strlen(text);
  if (len == 0) return true;
  size_t written = _file.write((const uint8_t*)text, len);
  if (written != len) return false;
  _bytesWritten += written;
  return true;
}

bool GpxWriter::_writeFormat(const char* format, ...) {
  if (!_file || !format) return false;
  char buf[160];
  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (len < 0 || len >= (int)sizeof(buf)) return false;
  return _writeRaw(buf);
}

void GpxWriter::_formatUtcISO(char* buf, int bufSize,
                              int year, int month, int day,
                              int hour, int minute, float second) {
  int secInt = (int)second;
  snprintf(buf, bufSize,
           "%04d-%02d-%02dT%02d:%02d:%02dZ",
           year, month, day, hour, minute, secInt);
}

bool GpxWriter::_makeUniquePaths(int year, int month, int day,
                                 int hour, int minute, int second) {
  for (int suffix = 0; suffix <= 999; suffix++) {
    if (suffix == 0) {
      snprintf(_fileName, sizeof(_fileName),
               "track_%04d%02d%02d_%02d%02d%02d.gpx",
               year, month, day, hour, minute, second);
    } else {
      snprintf(_fileName, sizeof(_fileName),
               "track_%04d%02d%02d_%02d%02d%02d_%03d.gpx",
               year, month, day, hour, minute, second, suffix);
    }

    snprintf(_filePath, sizeof(_filePath), GPX_TRACK_DIR "/%s", _fileName);
    snprintf(_tmpFilePath, sizeof(_tmpFilePath), "%s.tmp", _filePath);
    if (!SD.exists(_filePath) && !SD.exists(_tmpFilePath)) {
      return true;
    }
  }
  return false;
}

bool GpxWriter::_flushIfDue(bool force) {
  if (!_file) return false;
  unsigned long now = millis();
  if (force ||
      _pointsSinceFlush >= GPX_FLUSH_EVERY_N_POINTS ||
      now - _lastFlushMs >= GPX_FLUSH_INTERVAL_MS) {
    _file.flush();
    if (!_file || _file.position() < _bytesWritten ||
        _file.size() < _bytesWritten) {
      return false;
    }
    _pointsSinceFlush = 0;
    _lastFlushMs = now;
  }
  return true;
}

void GpxWriter::_setError(const char* message) {
  strncpy(_lastError, message, sizeof(_lastError) - 1);
  _lastError[sizeof(_lastError) - 1] = '\0';
}

void GpxWriter::_clearError() {
  _lastError[0] = '\0';
  if (_state == GpxWriterState::Error) _state = GpxWriterState::Idle;
}

const char* GpxWriter::_fixText(int fixMode) const {
  if (fixMode == 3) return "3d";
  if (fixMode == 2) return "2d";
  return "none";
}

bool GpxWriter::_recoverIncompleteFiles() {
  File dir = SD.open(GPX_TRACK_DIR, FILE_READ);
  if (!dir) return false;

  GpxWriterState previous = _state;
  _state = GpxWriterState::Recovering;
  bool allOk = true;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    bool isDir = entry.isDirectory();
    String entryName = entry.name();
    entry.close();
    if (isDir) continue;

    String fullPath = entryName;
    if (!fullPath.startsWith("/")) {
      fullPath = String(GPX_TRACK_DIR) + "/" + fullPath;
    }

    if (fullPath.endsWith(".gpx.tmp")) {
      allOk = _recoverOneFile(fullPath.c_str(), true) && allOk;
    } else if (fullPath.endsWith(".gpx") &&
               (!_fileHasHeader(fullPath.c_str()) ||
                !_fileHasFooter(fullPath.c_str()))) {
      allOk = _recoverOneFile(fullPath.c_str(), false) && allOk;
    }
  }

  dir.close();
  _state = previous;
  return allOk;
}

bool GpxWriter::_recoverOneFile(const char* path, bool isTmp) {
  bool ok = _fileHasHeader(path) && (_fileHasFooter(path) || _appendFooterToPath(path));
  char newPath[96];

  if (ok) {
    if (isTmp) {
      String finalPath = String(path);
      finalPath.replace(".gpx.tmp", "_recovered.gpx");
      if (!SD.exists(finalPath.c_str()) && SD.rename(path, finalPath.c_str())) {
        return true;
      }
      return _renameWithSuffix(path, "_recovered.gpx", newPath, sizeof(newPath));
    }
    return true;
  }

  return _renameWithSuffix(path, ".corrupt", newPath, sizeof(newPath));
}

bool GpxWriter::_fileHasHeader(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  char head[161];
  size_t got = f.read((uint8_t*)head, sizeof(head) - 1);
  f.close();
  head[got] = '\0';
  return strstr(head, "<?xml") && strstr(head, "<gpx");
}

bool GpxWriter::_fileHasFooter(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  size_t size = f.size();
  size_t readLen = size > 256 ? 256 : size;
  if (size > readLen) {
    f.seek(size - readLen);
  }

  char tail[257];
  size_t got = f.read((uint8_t*)tail, readLen);
  f.close();
  tail[got] = '\0';
  return strstr(tail, "</trkseg>") && strstr(tail, "</trk>") && strstr(tail, "</gpx>");
}

bool GpxWriter::_appendFooterToPath(const char* path) {
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  size_t written = f.print(GPX_FOOTER);
  f.flush();
  f.close();
  return written > 0 && _fileHasFooter(path);
}

bool GpxWriter::_renameWithSuffix(const char* fromPath, const char* suffix,
                                  char* outPath, size_t outSize) {
  for (int i = 0; i <= 999; i++) {
    if (i == 0) {
      snprintf(outPath, outSize, "%s%s", fromPath, suffix);
    } else {
      snprintf(outPath, outSize, "%s_%03d%s", fromPath, i, suffix);
    }
    if (!SD.exists(outPath) && SD.rename(fromPath, outPath)) {
      return true;
    }
  }
  return false;
}

bool GpxWriter::_endsWith(const char* value, const char* suffix) const {
  size_t valueLen = strlen(value);
  size_t suffixLen = strlen(suffix);
  return valueLen >= suffixLen &&
         strcmp(value + valueLen - suffixLen, suffix) == 0;
}
