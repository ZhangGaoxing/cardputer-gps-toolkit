/**
 * fn_signal_bars.cpp — 卫星信噪比柱状图（固定柱宽 + 水平滚动）
 * 移植自 GPSInfo SCR_SIGNAL_BARS
 *
 * 卫星多时柱会重叠 — 使用固定 12px 柱宽，当总宽度超出屏幕时
 * 按 [,]/[/] 键水平滚动。底部提示栏会显示滚动状态。
 */
#include "fn_signal_bars.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../ui_helpers.h"

static const int CHART_X = 22;
static const int CHART_W = 212;
static const int BAR_W  = 12;   // 固定柱宽
static const int BAR_GAP = 2;   // 柱间距
static const int BAR_STEP = BAR_W + BAR_GAP;  // 单柱占用宽度(14px)

void FnSignalBars::onEnter() {
  _scrollX = 0;
  _tab = 0;
}

void FnSignalBars::onUpdate(bool force) {
  // Tab indicator
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, CONTENT_TOP + 1);
  cv.print(_tab == 0 ? "SNR" : "Constell");
  cv.setCursor(SCREEN_W - 40, CONTENT_TOP + 1);
  cv.print("[Tab]");

  if (_tab == 0) _drawBars();
  else           _drawConstellation();
}

bool FnSignalBars::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;
  // Tab 切换子页面
  if (event.key == 0x09) { _tab = (_tab + 1) % 2; return true; }
  // SNR 页面时 , / 键滚动
  if (_tab == 0) {
    if (event.key == ',') { _scrollX -= BAR_STEP * 2; return true; }
    if (event.key == '/') { _scrollX += BAR_STEP * 2; return true; }
  }
  return false;
}

void FnSignalBars::_drawBars() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  const auto& allSats = gps.satellites();

  // 收集可见且有信号的卫星
  std::vector<const SatData*> visSats;
  for (auto& sat : allSats) {
    if (sat.visible && sat.snr > 0) visSats.push_back(&sat);
  }

  if (visSats.empty()) {
    cv.setTextSize(1);
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(50, SCREEN_H / 2 - 4);
    cv.print("No satellites visible");
    return;
  }

  // 按系统+ID排序
  std::sort(visSats.begin(), visSats.end(), [](const SatData* a, const SatData* b) {
    if (a->system != b->system) return a->system < b->system;
    return a->id < b->id;
  });

  int numSats = visSats.size();
  int totalW = numSats * BAR_STEP - BAR_GAP; // 所有柱总宽度

  // 限制滚动偏移量
  int maxScroll = totalW - CHART_W;
  if (maxScroll < 0) maxScroll = 0;
  if (_scrollX < 0) _scrollX = 0;
  if (_scrollX > maxScroll) _scrollX = maxScroll;

  // 图例行（含卫星总数）
  int legY = 14;
  struct { const char* label; uint16_t color; } legend[] = {
    {"GP",TFT_GREEN},{"GL",TFT_RED},{"GA",TFT_CYAN},{"BD",TFT_YELLOW},{"QZ",TFT_MAGENTA}
  };
  int legX = CHART_X;
  for (int i = 0; i < 5; i++) {
    cv.fillRect(legX, legY, 6, 6, legend[i].color);
    cv.setTextColor(legend[i].color);
    cv.setCursor(legX + 8, legY - 1);
    cv.print(legend[i].label);
    legX += 8 + strlen(legend[i].label) * 6 + 6;
  }
  // 卫星总数放在图例右侧
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(legX + 4, legY - 1);
  cv.printf("%d sats", numSats);

  // 图表区域：图例下方开始，填满剩余空间
  int chartY = legY + 14;
  int chartH = SCREEN_H - chartY - 14;
  int baseY = chartY + chartH;

  // Y轴标签与参考线
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(2, chartY); cv.print("50");
  cv.setCursor(2, chartY + chartH / 2 - 4); cv.print("25");
  cv.setCursor(5, baseY - 8); cv.print("0");
  for (int v = 0; v <= 50; v += 25) {
    int ly = baseY - (v * chartH / 50);
    cv.drawLine(CHART_X, ly, CHART_X + CHART_W, ly, TFT_DARKGREY);
  }

  // 柱状图（带裁剪，只绘制屏幕内可见的柱）
  int startIdx = _scrollX / BAR_STEP;               // 从哪一根开始
  int offsetPx = _scrollX % BAR_STEP;                // 像素偏移
  int startX = CHART_X - offsetPx;

  for (int i = startIdx; i < numSats; i++) {
    int bx = startX + (i - startIdx) * BAR_STEP;
    if (bx >= CHART_X + CHART_W) break;              // 超出右边界

    auto* sat = visSats[i];
    int snr = constrain(sat->snr, 0, 50);
    int barH = (snr * chartH) / 50;
    uint16_t color = systemColor(sat->system);

    // 已使用卫星加白框（裁剪到绘图区域）
    if (sat->used && barH > 0) {
      int rx = max(bx - 1, CHART_X);
      int rw = min(bx + BAR_W + 1, CHART_X + CHART_W) - rx;
      cv.drawRect(rx, baseY - barH - 1, rw, barH + 2, TFT_WHITE);
    }
    if (barH > 0 && bx + BAR_W > CHART_X && bx < CHART_X + CHART_W) {
      cv.fillRect(bx, baseY - barH, BAR_W, barH, color);
    }

    // 柱顶 SNR 数值
    if (BAR_W >= 5 && snr > 0 && bx >= CHART_X && bx + BAR_W <= CHART_X + CHART_W) {
      char snrBuf[4]; snprintf(snrBuf, sizeof(snrBuf), "%d", snr);
      cv.setTextSize(1); cv.setTextColor(TFT_WHITE);
      cv.setCursor(bx, baseY - barH - 10);
      cv.print(snrBuf);
    }
  }

  // 底部ID标签区（先擦再画，防止溢出残留）
  cv.fillRect(CHART_X, baseY + 1, CHART_W, 12, TFT_BLACK);
  cv.setTextSize(1);
  for (int i = startIdx; i < numSats; i++) {
    int bx = startX + (i - startIdx) * BAR_STEP;
    if (bx >= CHART_X + CHART_W) break;
    // 柱体完全在左边界之外时不画标签
    if (bx + BAR_W < CHART_X + 4) continue;

    auto* sat = visSats[i];
    uint16_t color = systemColor(sat->system);
    cv.setTextColor(color);
    char idBuf[4]; snprintf(idBuf, sizeof(idBuf), "%d", sat->id);
    int txtW = strlen(idBuf) * 6;
    int txtX = bx + (BAR_W - txtW) / 2;
    // 裁剪到图表区域内
    if (txtX < CHART_X) txtX = CHART_X;
    if (txtX + txtW > CHART_X + CHART_W) txtX = CHART_X + CHART_W - txtW;
    cv.setCursor(txtX, baseY + 2);
    cv.print(idBuf);
  }
}

// ============================================================
//  Tab 1 — 星座详情（原 Constellation）
// ============================================================
void FnSignalBars::_drawConstellation() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  const auto& sats = gps.satellites();

  int gpV = 0, glV = 0, gaV = 0, bdV = 0, qzV = 0;
  int gpU = 0, glU = 0, gaU = 0, bdU = 0, qzU = 0;
  for (auto& sat : sats) {
    if (sat.used) {
      if (sat.system == "GPS") gpU++;
      else if (sat.system == "GLONASS") glU++;
      else if (sat.system == "Galileo") gaU++;
      else if (sat.system == "BeiDou") bdU++;
      else if (sat.system == "QZSS") qzU++;
    }
    if (sat.visible) {
      if (sat.system == "GPS") gpV++;
      else if (sat.system == "GLONASS") glV++;
      else if (sat.system == "Galileo") gaV++;
      else if (sat.system == "BeiDou") bdV++;
      else if (sat.system == "QZSS") qzV++;
    }
  }

  cv.setTextSize(1);
  int y = CONTENT_TOP + 12;
  int lh = 17;
  char buf[40];

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, y);
  cv.print("System     Visible  Used");
  y += lh;
  cv.drawLine(4, y - 2, 200, y - 2, TFT_DARKGREY);

  struct { const char* name; uint16_t color; int vis, used; } rows[] = {
    {"GPS",     TFT_GREEN,   gpV, gpU},
    {"GLONASS", TFT_RED,     glV, glU},
    {"Galileo", TFT_CYAN,    gaV, gaU},
    {"BeiDou",  TFT_YELLOW,  bdV, bdU},
    {"QZSS",    TFT_MAGENTA, qzV, qzU},
  };

  for (auto& r : rows) {
    cv.fillCircle(10, y + 3, 3, r.color);
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(18, y);
    snprintf(buf, sizeof(buf), "%-11s %3d     %3d", r.name, r.vis, r.used);
    cv.print(buf);
    y += lh;
  }

  cv.drawLine(4, y - 2, 200, y - 2, TFT_DARKGREY);
  int totalVis = gpV + glV + gaV + bdV + qzV;
  int totalUsed = gpU + glU + gaU + bdU + qzU;
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(18, y);
  snprintf(buf, sizeof(buf), "%-11s %3d     %3d", "Total", totalVis, totalUsed);
  cv.print(buf);
}

void FnSignalBars::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int pad = 3, bw = (size - pad * 3) / 4;
  int baseY = y + size - 2;
  // 4 ascending bars with varying heights
  int heights[4] = {size*2/5, size*3/5, size/2, size*4/5};
  for (int i = 0; i < 4; i++) {
    int bx = x + pad + i * (bw + pad);
    cv.fillRect(bx, baseY - heights[i], bw, heights[i], color);
    // Thin white outline on each bar for definition
    cv.drawRect(bx, baseY - heights[i], bw, heights[i], color);
  }
}
