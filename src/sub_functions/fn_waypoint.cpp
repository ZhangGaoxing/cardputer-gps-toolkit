/**
 * fn_waypoint.cpp — 航点管理页面
 *
 * 功能：列表浏览、详情查看、创建航点（GPS）、删除确认
 * 子页面导航：List ↔ Detail, List ↔ Create, List ↔ DeleteConfirm
 * ESC 在非 List 页返回 List，在 List 页返回菜单
 *
 * 风格参考 Trip 页面：TFT_GREEN 标签、TFT_WHITE/TFT_CYAN 值、紧凑行高
 * 适配 Cardputer ADV 小屏幕（240×135）
 */
#include "fn_waypoint.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../waypoint_manager.h"
#include "../navigation_manager.h"
#include "../sd_manager.h"
#include "../ui_helpers.h"
#include "../geo_math.h"

#include <string.h>

// ==================================================================
//  类型/来源名称映射
// ==================================================================
static const char* typeName(uint8_t t) {
  switch ((WaypointType)t) {
    case WP_TYPE_CAMP:      return "Camp";
    case WP_TYPE_WATER:     return "Water";
    case WP_TYPE_DANGER:    return "Danger";
    case WP_TYPE_SUMMIT:    return "Summit";
    case WP_TYPE_VIEWPOINT: return "View";
    case WP_TYPE_TRAILHEAD: return "Trail";
    default:                return "Point";
  }
}

static const char* sourceName(uint8_t s) {
  switch ((WaypointSource)s) {
    case WP_SRC_CURRENT_FIX: return "GPS Fix";
    case WP_SRC_MAP_CENTER:  return "Map Ctr";
    case WP_SRC_MANUAL:      return "Manual";
    case WP_SRC_IMPORTED:    return "Import";
    default:                 return "?";
  }
}

static void formatDist(char* buf, int bufSize, float distKm) {
  if (distKm < 1.0f) snprintf(buf, bufSize, "%.0f m", distKm * 1000.0f);
  else if (distKm < 100.0f) snprintf(buf, bufSize, "%.2f km", distKm);
  else snprintf(buf, bufSize, "%.1f km", distKm);
}

// 类型名称表（静态成员）
const char* FnWaypoint::kTypeNames[] = {
  "Point", "Camp", "Water", "Danger", "Summit", "View", "Trail"
};
const int FnWaypoint::kTypeCount = 7;

// ==================================================================
//  生命周期
// ==================================================================
void FnWaypoint::onEnter() {
  _page = PAGE_LIST;
  _selectedIdx = 0;
  _scrollOffset = 0;
  _detailWpId = 0;
  _nameLen = 0;
  _editingName = false;
  _wpType = 0;
  _noteLen = 0;
  _editingNote = false;
  memset(_newName, 0, sizeof(_newName));
  memset(_newNote, 0, sizeof(_newNote));
  WaypointManager::instance().load(); // 刷新航点数据（而非 begin，避免重置内部状态）
  _dirty = true;
}

// ==================================================================
//  主绘制 — 始终绘制（与 Trip 一致，不做 _dirty 跳过）
// ==================================================================
void FnWaypoint::onUpdate(bool force) {
  (void)force;
  M5Canvas& cv = DisplayManager::instance().canvas();
  cv.fillScreen(TFT_BLACK);

  // 页签头（参考 Trip 风格：左侧页名，右侧无杂项）
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 2);
  switch (_page) {
    case PAGE_LIST:           cv.print("Waypoints"); break;
    case PAGE_DETAIL:         cv.print("Detail");    break;
    case PAGE_CREATE:         cv.print("New");       break;
    case PAGE_DELETE_CONFIRM: cv.print("Delete");    break;
  }

  switch (_page) {
    case PAGE_LIST:           _drawList();           break;
    case PAGE_DETAIL:         _drawDetail();         break;
    case PAGE_CREATE:         _drawCreate();         break;
    case PAGE_DELETE_CONFIRM: _drawDeleteConfirm();  break;
  }
  _dirty = false;
}

// ==================================================================
//  按键处理
// ==================================================================
bool FnWaypoint::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed) return false;

  WaypointManager& wm = WaypointManager::instance();
  size_t wpCount = wm.count();

  switch (_page) {

    // ======== 列表页 ========
    case PAGE_LIST: {
      int maxIdx = (int)wpCount - 1;

      switch (event.key) {
        case ';':  // 下
          if (wpCount > 0) {
            int maxSel = maxIdx;
            if (_selectedIdx > 0) { _selectedIdx--;
              if (_selectedIdx < _scrollOffset) _scrollOffset--; }
            else { _selectedIdx = maxIdx;
              _scrollOffset = (maxIdx >= ITEMS_PER_PAGE) ? maxIdx - ITEMS_PER_PAGE + 1 : 0; }
          }
          _dirty = true; return true;

        case '.':  // 上
          if (wpCount > 0) {
            if (_selectedIdx < maxIdx) { _selectedIdx++;
              if (_selectedIdx >= _scrollOffset + ITEMS_PER_PAGE) _scrollOffset++; }
            else { _selectedIdx = 0; _scrollOffset = 0; }
          }
          _dirty = true; return true;

        case 0x0D:  // Enter → 查看详情
          if (wpCount > 0) {
            const Waypoint* wp = wm.getByIndex(_selectedIdx);
            if (wp) { _detailWpId = wp->id; _page = PAGE_DETAIL; _dirty = true; }
          }
          return true;

        case 'n': case 'N':  // 新建
          _page = PAGE_CREATE; _nameLen = 0; _editingName = false;
          _wpType = 0; _noteLen = 0; _editingNote = false;
          memset(_newName, 0, sizeof(_newName));
          memset(_newNote, 0, sizeof(_newNote)); _dirty = true;
          return true;

        case 'd': case 'D':  // 删除确认
          if (wpCount > 0) {
            _detailWpId = 0; // 从列表进入删除确认，不使用可能残留的详情 ID
            _page = PAGE_DELETE_CONFIRM; _dirty = true;
          }
          return true;

        case 'r': case 'R':  // 刷新
          wm.load(); _selectedIdx = 0; _scrollOffset = 0; _dirty = true;
          return true;

        default: return false;
      }
    }

    // ======== 详情页 ========
    case PAGE_DETAIL: {
      if (event.key == '`' || event.key == 0x1B) {
        _detailWpId = 0; // 回到列表时清除详情追踪，修复删除目标错位 bug
        _page = PAGE_LIST; _dirty = true; return true;  // ESC → 返回列表（非菜单）
      }
      if (event.key == 'd' || event.key == 'D') {
        _page = PAGE_DELETE_CONFIRM; _dirty = true; return true;
      }
      if (event.key == 'g' || event.key == 'G') {
        NavigationManager& nav = NavigationManager::instance();
        if (nav.isTarget((uint16_t)_detailWpId)) {
          nav.stop("Canceled");
        } else {
          nav.startGoto((uint16_t)_detailWpId);
        }
        _dirty = true; return true;
      }
      return false;
    }

    // ======== 创建页 ========
    case PAGE_CREATE: {
      if (event.key == '`' || event.key == 0x1B) {
        _page = PAGE_LIST; _dirty = true; return true;  // ESC → 返回列表（非菜单）
      }
      if (event.key == 'g' || event.key == 'G') {
        _doCreateFromGps(); return true;
      }

      // 编辑名称
      if (_editingName) {
        if (event.key == 0x0D) { _editingName = false; _dirty = true; return true; }
        if (event.key == 0x7F) { if (_nameLen > 0) { _nameLen--; _newName[_nameLen] = '\0'; } _dirty = true; return true; }
        if (event.key >= 0x20 && event.key <= 0x7E) {
          if (_nameLen < WAYPOINT_NAME_MAX_LEN - 1) { _newName[_nameLen++] = event.key; _newName[_nameLen] = '\0'; }
          _dirty = true; return true;
        }
        return false;
      }

      // 编辑备注
      if (_editingNote) {
        if (event.key == 0x0D) { _editingNote = false; _dirty = true; return true; }
        if (event.key == 0x7F) { if (_noteLen > 0) { _noteLen--; _newNote[_noteLen] = '\0'; } _dirty = true; return true; }
        if (event.key >= 0x20 && event.key <= 0x7E) {
          if (_noteLen < WAYPOINT_NOTE_MAX_LEN - 1) { _newNote[_noteLen++] = event.key; _newNote[_noteLen] = '\0'; }
          _dirty = true; return true;
        }
        return false;
      }

      // 非编辑模式导航
      if (event.key == ';' || event.key == '.') {
        // 切换类型
        int dir = (event.key == '.') ? 1 : -1;
        _wpType = (_wpType + dir + kTypeCount) % kTypeCount;
        _dirty = true; return true;
      }
      if (event.key == 0x09) {  // Tab → 编辑名称
        _editingName = true; _dirty = true; return true;
      }
      if (event.key == ' ') {   // Space → 编辑备注
        _editingNote = true; _dirty = true; return true;
      }
      return false;
    }

    // ======== 删除确认 ========
    case PAGE_DELETE_CONFIRM: {
      if (event.key == 'y' || event.key == 'Y') { _doDeleteSelected(); return true; }
      if (event.key == 'n' || event.key == 'N' || event.key == '`' || event.key == 0x1B) {
        _page = (_detailWpId != 0) ? PAGE_DETAIL : PAGE_LIST; _dirty = true; return true;
      }
      return false;
    }
  }
  return false;
}

// ==================================================================
//  列表页绘制
// ==================================================================
void FnWaypoint::_drawList() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  WaypointManager& wm = WaypointManager::instance();
  GPSManager& gps = GPSManager::instance();
  NavigationManager& nav = NavigationManager::instance();
  size_t wpCount = wm.count();

  int y = 14, lh = 12;

  // 航点数
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, y);
  cv.printf("%u/%u waypoints", (unsigned)wpCount, (unsigned)WAYPOINT_MAX_COUNT);
  y += lh + 2;

  // 空列表：居中提示
  if (wpCount == 0) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setTextSize(1);
    // 水平居中
    const char* msg1 = "No waypoints";
    const char* msg2 = "Press [n] to add one";
    int w1 = strlen(msg1) * 6;
    int w2 = strlen(msg2) * 6;
    int centerY = SCREEN_H / 2 - 10;
    cv.setCursor((SCREEN_W - w1) / 2, centerY);
    cv.print(msg1);
    cv.setCursor((SCREEN_W - w2) / 2, centerY + 14);
    cv.print(msg2);
    // 底部提示
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, SCREEN_H - 10);
    cv.print("[;][.]Nav  [Ent]View  [n]Add");
    return;
  }

  // 表格标题
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  cv.print("Name");
  cv.setCursor(80, y);
  cv.print("Dist");
  cv.setCursor(130, y);
  cv.print("Brg");
  cv.setCursor(170, y);
  cv.print("Type");
  cv.setCursor(210, y);
  cv.print("ID");
  cv.drawLine(0, y + 10, SCREEN_W - 1, y + 10, TFT_DARKGREY);
  y += 12;

  int rowH = 16;
  int end = (int)(_scrollOffset + ITEMS_PER_PAGE);
  if (end > (int)wpCount) end = (int)wpCount;

  for (int i = (int)_scrollOffset; i < end; i++) {
    const Waypoint* wp = wm.getByIndex(i);
    if (!wp) continue;
    int ry = y + (i - _scrollOffset) * rowH;
    bool selected = (i == _selectedIdx);

    if (selected) {
      cv.fillRect(0, ry, SCREEN_W, rowH - 1, UI_ACTIVE);
      cv.setTextColor(TFT_BLACK);
    } else {
      cv.setTextColor(TFT_WHITE);
    }

    // 名称（截断到7字符）
    cv.setCursor(4, ry + 2);
    cv.print(nav.isTarget(wp->id) ? ">" : " ");
    cv.setCursor(10, ry + 2);
    char nb[WAYPOINT_NAME_MAX_LEN];
    strncpy(nb, wp->name, 7); nb[7] = '\0';
    cv.print(nb);

    // 距离
    cv.setCursor(80, ry + 2);
    if (gps.hasReliableFix()) {
      float dist = wm.distanceToWaypoint(wp->id, gps.latitude(), gps.longitude());
      char db[12]; formatDist(db, sizeof(db), dist); cv.print(db);
    } else cv.print("---");

    // 方位角
    cv.setCursor(130, ry + 2);
    if (gps.hasReliableFix()) {
      float brg = wm.bearingToWaypoint(wp->id, gps.latitude(), gps.longitude());
      cv.printf("%.0f%s", brg, cardinalFromHeading(brg));
    } else cv.print("---");

    // 类型
    cv.setCursor(170, ry + 2);
    cv.print(typeName(wp->type));

    // ID
    cv.setCursor(210, ry + 2);
    cv.print(wp->id);
  }

  // 底部操作提示
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[;][.]Move [Ent]View [n]Add [d]Del");

  // 加载错误
  if (wm.loadErrors() > 0) {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(180, SCREEN_H - 10);
    cv.printf("skip:%d", wm.loadErrors());
  }
}

// ==================================================================
//  详情页绘制
// ==================================================================
void FnWaypoint::_drawDetail() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  WaypointManager& wm = WaypointManager::instance();
  GPSManager& gps = GPSManager::instance();
  NavigationManager& nav = NavigationManager::instance();

  const Waypoint* wp = wm.getById(_detailWpId);
  if (!wp) { _page = PAGE_LIST; _dirty = true; return; }

  int y = 14, lh = 13;
  char buf[48];
  cv.setTextSize(1);

  // 名称
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Name: %s", wp->name);
  cv.print(buf); y += lh + 2;
  if (nav.isTarget(wp->id)) {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(142, 14);
    cv.print("GO-TO");
  }

  // 坐标
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Lat: %.6f  Lon: %.6f", wp->lat, wp->lon);
  cv.print(buf); y += lh;

  // 海拔
  if (wp->ele != 0.0f) {
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Alt: %.0f m", wp->ele);
    cv.print(buf); y += lh;
  }

  y += 1;
  // 距离/方位（需GPS）
  if (gps.hasReliableFix()) {
    float dist = wm.distanceToWaypoint(wp->id, gps.latitude(), gps.longitude());
    float brg  = wm.bearingToWaypoint(wp->id, gps.latitude(), gps.longitude());
    cv.setTextColor(TFT_CYAN);
    cv.setCursor(4, y);
    char db[16]; formatDist(db, sizeof(db), dist);
    snprintf(buf, sizeof(buf), "Dist: %s", db);
    cv.print(buf); y += lh;
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Brg: %.0f%s", brg, cardinalFromHeading(brg));
    cv.print(buf); y += lh;
  } else {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, y);
    cv.print("No GPS fix"); y += lh;
    cv.setCursor(4, y);
    cv.print("Dist/Brg unavailable"); y += lh;
  }

  y += 1;
  // 时间
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  cv.print("Created:"); y += lh;
  cv.setTextColor(TFT_WHITE);
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           wp->year, wp->month, wp->day, wp->hour, wp->minute, wp->second);
  cv.print(buf); y += lh;

  // 来源/类型
  cv.setCursor(4, y);
  snprintf(buf, sizeof(buf), "Src: %s  Type: %s", sourceName(wp->source), typeName(wp->type));
  cv.print(buf); y += lh;

  // 备注
  if (wp->note[0] != '\0') {
    y += 1;
    cv.setTextColor(TFT_GREEN);
    cv.setCursor(4, y);
    cv.print("Note:"); y += lh;
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(4, y);
    cv.print(wp->note);
  }

  // 底部操作提示
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print(nav.isTarget(wp->id) ? "[g]Stop  [d]Delete"
                                : "[g]Go-to [d]Delete");
}

// ==================================================================
//  创建页绘制
// ==================================================================
void FnWaypoint::_drawCreate() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  WaypointManager& wm = WaypointManager::instance();

  int y = 14, lh = 13;
  cv.setTextSize(1);

  // === 名称行 ===
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  cv.print("Name:");
  cv.setTextColor(_editingName ? TFT_YELLOW : TFT_WHITE);
  cv.setCursor(50, y);
  if (_nameLen == 0) cv.print("(edit)");
  else cv.print(_newName);
  if (_editingName) cv.print("_");
  y += lh + 2;

  // === 类型行 ===
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  cv.print("Type:");
  cv.setTextColor((_editingName || _editingNote) ? TFT_DARKGREY : TFT_WHITE);
  cv.setCursor(50, y);
  cv.printf("< %s >", kTypeNames[_wpType]);
  y += lh + 2;

  // === 备注行 ===
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y);
  cv.print("Note:");
  cv.setTextColor(_editingNote ? TFT_YELLOW : TFT_WHITE);
  cv.setCursor(50, y);
  if (_noteLen == 0) cv.print("(opt)");
  else cv.print(_newNote);
  if (_editingNote) cv.print("_");
  y += lh + 4;

  // === GPS 状态 ===
  cv.setTextColor(TFT_GREEN);
  cv.setCursor(4, y); y += lh;
  cv.print("GPS:");

  if (gps.hasReliableFix()) {
    char buf[56];
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "%.6f, %.6f", gps.latitude(), gps.longitude());
    cv.print(buf); y += lh;
    cv.setCursor(4, y);
    snprintf(buf, sizeof(buf), "Alt: %.0fm  Sat: %d  HDOP: %.1f",
             gps.altitude(), gps.satellitesUsed(), gps.hdop());
    cv.print(buf);
  } else {
    cv.setTextColor(TFT_RED);
    cv.setCursor(4, y);
    cv.print("No reliable fix"); y += lh;
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(4, y);
    cv.print("Wait for fix, then press [g]");
  }

  // 容量满提示
  if (wm.isFull()) {
    cv.setTextColor(TFT_RED);
    cv.setCursor(4, SCREEN_H - 22);
    cv.print("List full! Delete some first.");
  }

  // 底部操作提示
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 10);
  cv.print("[g]Save GPS  [Sp]Note  [;][.]Type");
}

// ==================================================================
//  删除确认页绘制
// ==================================================================
void FnWaypoint::_drawDeleteConfirm() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  WaypointManager& wm = WaypointManager::instance();

  const Waypoint* wp = (_detailWpId != 0)
    ? wm.getById(_detailWpId)
    : wm.getByIndex(_selectedIdx);
  NavigationManager& nav = NavigationManager::instance();

  cv.setTextSize(1);
  cv.setTextColor(TFT_RED);
  cv.setCursor(30, 28);
  cv.print("Delete waypoint?");

  cv.setTextColor(TFT_WHITE);
  cv.setCursor(40, 48);
  if (wp) { cv.printf("ID:%u  %s", wp->id, wp->name); }
  else cv.print("(unknown)");

  cv.setTextColor(TFT_YELLOW);
  cv.setCursor(30, 70);
  cv.print("[y] Confirm delete");

  if (wp && nav.isTarget(wp->id)) {
    cv.setTextColor(TFT_YELLOW);
    cv.setCursor(30, 98);
    cv.print("Also stops Go-to");
  }

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(30, 84);
  cv.print("[n] Cancel");

  if (wm.lastError()[0] != '\0') {
    cv.setTextColor(TFT_RED);
    cv.setCursor(30, 112);
    cv.print(wm.lastError());
  }
}

// ==================================================================
//  操作实现
// ==================================================================
void FnWaypoint::_doCreateFromGps() {
  GPSManager& gps = GPSManager::instance();
  if (!gps.hasReliableFix()) { _dirty = true; return; }

  WaypointManager& wm = WaypointManager::instance();
  if (wm.isFull()) { _dirty = true; return; }

  char name[WAYPOINT_NAME_MAX_LEN];
  if (_nameLen > 0) {
    strncpy(name, _newName, WAYPOINT_NAME_MAX_LEN - 1);
    name[WAYPOINT_NAME_MAX_LEN - 1] = '\0';
  } else {
    snprintf(name, sizeof(name), "WP%03u", wm.nextId());
  }

  const Waypoint* wp = wm.addWaypoint(
    name,
    gps.latitude(), gps.longitude(), gps.altitude(),
    WP_SRC_CURRENT_FIX, (WaypointType)_wpType, _newNote,
    gps.utcYear(), gps.utcMonth(), gps.utcDay(),
    gps.utcHour(), gps.utcMinute(), gps.utcSecond()
  );

  if (wp) {
    _page = PAGE_LIST;
    _selectedIdx = (int)wm.count() - 1;
    _scrollOffset = (_selectedIdx >= ITEMS_PER_PAGE) ? _selectedIdx - ITEMS_PER_PAGE + 1 : 0;
  }
  _dirty = true;
}

void FnWaypoint::_doDeleteSelected() {
  WaypointManager& wm = WaypointManager::instance();
  uint16_t deleteId;
  if (_detailWpId != 0) {
    deleteId = _detailWpId;
  } else {
    const Waypoint* wp = wm.getByIndex(_selectedIdx);
    deleteId = wp ? wp->id : 0;
    if (!wp) { _page = PAGE_LIST; _dirty = true; return; } // 防御性检查
    deleteId = wp->id;
  }
  if (wm.deleteWaypoint(deleteId)) {
    if (NavigationManager::instance().isTarget(deleteId)) {
      NavigationManager::instance().stop("Target deleted");
    }
    _page = PAGE_LIST;
    _detailWpId = 0;
    if (_selectedIdx >= (int)wm.count() && wm.count() > 0) {
      _selectedIdx = (int)wm.count() - 1;
    }
    _scrollOffset = (_selectedIdx >= ITEMS_PER_PAGE) ? _selectedIdx - ITEMS_PER_PAGE + 1 : 0;
  }
  _dirty = true;
}

// ==================================================================
//  图标绘制
// ==================================================================
void FnWaypoint::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2;
  cv.fillCircle(cx, cy - size / 5, size / 3, color);
  cv.fillTriangle(cx - 4, cy - size / 5 + 2, cx + 4, cy - size / 5 + 2, cx, cy + size / 3, color);
  cv.fillCircle(cx, cy - size / 5, 2, TFT_BLACK);
}
