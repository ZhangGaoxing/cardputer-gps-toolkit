/**
 * fn_twilight.h — 晨昏蒙影与日月出没计算
 *
 * 页面 0 (Solar)：日出/日落、民用/航海/天文蒙影、昼夜长度、正午
 * 页面 1 (Lunar) ：月出/月落、月相名称、月龄、照度
 *
 * 算法来源：
 *   日照 — NOAA Solar Calculator（精度 ~1 min）
 *   月球 — Jean Meeus 《Astronomical Algorithms》简化版（精度 ~2 min）
 */
#ifndef FN_TWILIGHT_H
#define FN_TWILIGHT_H

#include "sub_function.h"

class FnTwilight : public SubFunction {
public:
  FnTwilight() : SubFunction("Twilight", ICON_TWILIGHT) { setUpdateInterval(200); }

  void onEnter() override;
  void onUpdate(bool force) override;
  bool needsRedraw(unsigned long now) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  int _page = 0;   // 0 = solar, 1 = lunar
  bool _dirty = true;

  // ──────────────────────────────────────────────
  //  数据结构
  // ──────────────────────────────────────────────
  struct SunTimes {
    int sunriseMin;     // UTC 分钟，-1 表示无法计算
    int sunsetMin;
    int civilDawnMin;
    int civilDuskMin;
    int nautDawnMin;
    int nautDuskMin;
    int astroDawnMin;
    int astroDuskMin;
    int solarNoonMin;
    bool alwaysUp;      // 极昼
    bool alwaysDown;    // 极夜
  };

  struct MoonInfo {
    int  moonriseMin;   // UTC 分钟，-1 = 今日不出
    int  moonsetMin;    // UTC 分钟，-1 = 今日不落
    float agedays;      // 月龄（0–29.53 天）
    float illumination; // 照度 0–1
    bool  noRise;
    bool  noSet;
  };

  // ──────────────────────────────────────────────
  //  天文计算（静态辅助）
  // ──────────────────────────────────────────────

  /** 格里历日期 → 儒略日（0h UT） */
  static double _calcJD(int y, int m, int d);

  /**
   * 计算给定天顶角下的日出/日落（NOAA 算法）
   * zenithDeg: 90.833=日出, 96=民用, 102=航海, 108=天文
   * 返回 UTC 分钟；极昼/极夜时设 alwaysUp/alwaysDown 并将 *riseMin/*setMin=-1
   */
  static void _calcSunEvent(double jd, double latDeg, double lonDeg,
                             double zenithDeg,
                             int* riseMin, int* setMin,
                             bool* alwaysUp, bool* alwaysDown);

  /** 计算当日全部太阳时刻 */
  static SunTimes _calcAllSunTimes(double jd, double latDeg, double lonDeg);

  /** 计算某 JD（含小数时刻）月球高度角（度） */
  static double _moonAltitude(double jd, double latDeg, double lonDeg);

  /** 计算月出/月落及月相信息 */
  static MoonInfo _calcMoonInfo(double jd, double latDeg, double lonDeg);

  // ──────────────────────────────────────────────
  //  绘图
  // ──────────────────────────────────────────────
  void _drawSolarPage(const SunTimes& st, bool hasPos,
                      double lat, double lon, int tzOff);
  void _drawLunarPage(const MoonInfo& mi, bool hasPos, int tzOff);

  /** UTC 分钟 + 时区偏移 → "HH:MM"（超出当日范围正常折叠） */
  static void _fmtLocal(int utcMin, int tzOff, char* buf, int len);

  /** 分钟 → "Xh XXm" */
  static void _fmtDuration(int minutes, char* buf, int len);

  /** 月相名称 */
  static const char* _moonPhaseName(float ageDays);
};

#endif // FN_TWILIGHT_H
