/**
 * gpx_writer.h - GPX track file writer
 *
 * Writes accepted trip points to SD card using a temporary file first, then
 * finalizes to .gpx after a clean footer/flush/close.
 */
#ifndef GPX_WRITER_H
#define GPX_WRITER_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include "config.h"

enum class GpxWriterState {
  Idle,
  Recording,
  Error,
  Closed,
  Recovering
};

struct GpxTrackPoint {
  float lat;
  float lon;
  float altM;
  float speedKmph;
  float courseDeg;
  float hdop;
  float pdop;
  float vdop;
  int satellites;
  int fixMode;
  int fixQuality;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  float second;
};

class GpxWriter {
public:
  static GpxWriter& instance();

  bool begin();
  bool isRecording() const { return _state == GpxWriterState::Recording; }
  GpxWriterState state() const { return _state; }
  const char* lastError() const { return _lastError; }

  bool startRecording(int year, int month, int day,
                      int hour, int minute, int second);
  bool startRecording(int year, int month, int day,
                      int hour, int minute) {
    return startRecording(year, month, day, hour, minute, 0);
  }

  bool stopRecording();
  bool startNewSegment();
  bool appendTrackPoint(const GpxTrackPoint& point);

  void setRecordIntervalIndex(uint8_t index);
  uint8_t recordIntervalIndex() const { return _recordIntervalIndex; }
  unsigned long recordIntervalMs() const;
  const char* recordIntervalLabel() const;

  int pointCount() const { return _pointCount; }
  unsigned long recordingStartMs() const { return _recordingStartMs; }
  const char* currentFileName() const { return _fileName; }
  bool sdReady() const { return _sdReady; }

private:
  GpxWriter() = default;
  GpxWriter(const GpxWriter&) = delete;
  GpxWriter& operator=(const GpxWriter&) = delete;

  bool _writeHeader();
  bool _writeFooter();
  bool _writeRaw(const char* text);
  bool _writeFormat(const char* format, ...);
  void _formatUtcISO(char* buf, int bufSize,
                     int year, int month, int day,
                     int hour, int minute, float second);

  bool _makeUniquePaths(int year, int month, int day,
                        int hour, int minute, int second);
  bool _flushIfDue(bool force);
  void _setError(const char* message);
  void _clearError();
  const char* _fixText(int fixMode) const;
  bool _recoverIncompleteFiles();
  bool _recoverOneFile(const char* path, bool isTmp);
  bool _fileHasHeader(const char* path);
  bool _fileHasFooter(const char* path);
  bool _appendFooterToPath(const char* path);
  bool _renameWithSuffix(const char* fromPath, const char* suffix,
                         char* outPath, size_t outSize);
  bool _endsWith(const char* value, const char* suffix) const;

  bool _sdReady = false;
  GpxWriterState _state = GpxWriterState::Idle;
  File _file;
  char _fileName[48] = "";
  char _filePath[72] = "";
  char _tmpFilePath[76] = "";
  char _lastError[80] = "";
  int _pointCount = 0;
  int _pointsSinceFlush = 0;
  size_t _bytesWritten = 0;
  unsigned long _recordingStartMs = 0;
  unsigned long _lastFlushMs = 0;
  uint8_t _recordIntervalIndex = GPX_RECORD_INTERVAL_DEFAULT_INDEX;
};

#endif
