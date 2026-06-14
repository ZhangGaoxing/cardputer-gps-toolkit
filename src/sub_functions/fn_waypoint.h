/**
 * fn_waypoint.h — 航点管理页面（列表 + 详情 + 创建 + 基础导航）
 */
#ifndef FN_WAYPOINT_H
#define FN_WAYPOINT_H
#include "sub_function.h"
#include <Arduino.h>

class FnWaypoint : public SubFunction {
public:
  FnWaypoint() : SubFunction("Waypoint", ICON_WAYPOINT) { setUpdateInterval(500); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  bool wantsEsc() override { return _page != PAGE_LIST; }  // 子页时拦截ESC，列表页允许退出到菜单
  void drawIcon(int x, int y, int size, uint16_t color) override;
private:
  enum Page {
    PAGE_LIST = 0,      // 航点列表
    PAGE_DETAIL,        // 航点详情
    PAGE_CREATE,        // 创建航点
    PAGE_DELETE_CONFIRM // 删除确认
  };

  Page _page = PAGE_LIST;
  int  _selectedIdx = 0;
  int  _scrollOffset = 0;
  int  _detailWpId = 0;
  bool _dirty = true;

  // 创建航点状态
  char _newName[WAYPOINT_NAME_MAX_LEN] = "";
  int  _nameLen = 0;
  bool _editingName = false;
  int  _wpType = 0;       // WaypointType 枚举索引
  char _newNote[WAYPOINT_NOTE_MAX_LEN] = "";
  int  _noteLen = 0;
  bool _editingNote = false;

  // 类型名称表
  static const char* kTypeNames[];
  static const int   kTypeCount;

  // 每页显示数量（适配小屏幕）
  static const int ITEMS_PER_PAGE = 5;

  void _drawList();
  void _drawDetail();
  void _drawCreate();
  void _drawDeleteConfirm();

  void _doCreateFromGps();
  void _doDeleteSelected();
};

#endif
