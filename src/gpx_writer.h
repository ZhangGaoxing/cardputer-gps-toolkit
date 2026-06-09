/**
 * gpx_writer.h — GPX轨迹文件写入器
 *
 * 将GPS轨迹点实时追加写入SD卡上的.GPX文件。
 * 每次写入trkpt后立即调用file.flush()防止断电数据丢失。
 * 文件名根据当前GPS日期时间动态生成（Trip_20260609_0830.gpx）。
 */
#ifndef GPX_WRITER_H
#define GPX_WRITER_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>

class GpxWriter {
public:
  static GpxWriter& instance();

  /** 确保SD卡已初始化（调用SD.begin） */
  bool begin();

  /** 是否正在录制中 */
  bool isRecording() const { return _recording; }

  /**
   * 开始一段新轨迹记录
   * 根据当前GPS UTC日期时间创建GPX文件并写入XML头部。
   * @param year, month, day, hour, minute  UTC日期时间
   * @return 成功开始录制返回true
   */
  bool startRecording(int year, int month, int day,
                      int hour, int minute);

  /**
   * 停止录制：写入闭合标签并安全关闭文件
   */
  void stopRecording();

  /**
   * 追加一个轨迹点到GPX文件
   * 写入后立即调用 file.flush() 强制刷入SD卡物理扇区。
   * @param lat  纬度（度）
   * @param lon  经度（度）
   * @param altM 海拔（米）
   * @param speedKmph 速度（km/h）
   * @param year, month, day, hour, minute, second UTC时间
   */
  void appendTrackPoint(float lat, float lon, float altM,
                        float speedKmph,
                        int year, int month, int day,
                        int hour, int minute, float second);

  /** 获取已记录的轨迹点数量 */
  int pointCount() const { return _pointCount; }

  /** 获取录制开始时的毫秒时间戳 */
  unsigned long recordingStartMs() const { return _recordingStartMs; }

  /** 获取当前GPX文件名（仅文件名，不含路径） */
  const char* currentFileName() const { return _fileName; }

  /** SD卡是否就绪 */
  bool sdReady() const { return _sdReady; }

private:
  GpxWriter() = default;
  GpxWriter(const GpxWriter&) = delete;
  GpxWriter& operator=(const GpxWriter&) = delete;

  /** 写入XML头部 */
  bool _writeHeader();

  /** 写入闭合标签 */
  void _writeFooter();

  /** 格式化UTC时间为ISO 8601字符串 (YYYY-MM-DDTHH:MM:SSZ) */
  void _formatUtcISO(char* buf, int bufSize,
                     int year, int month, int day,
                     int hour, int minute, float second);

  bool _sdReady = false;
  bool _recording = false;
  File _file;
  char _fileName[32];          // e.g. "Trip_20260609_0830.gpx"
  char _filePath[48];          // e.g. "/gpsmap/Trip_20260609_0830.gpx"
  int _pointCount = 0;
  unsigned long _recordingStartMs = 0;
};

#endif
