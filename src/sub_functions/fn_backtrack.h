/**
 * fn_backtrack.h - Backtrack navigation page.
 */
#ifndef FN_BACKTRACK_H
#define FN_BACKTRACK_H

#include "sub_function.h"

class FnBacktrack : public SubFunction {
public:
  FnBacktrack() : SubFunction("Backtrack", ICON_BACKTRACK) { setUpdateInterval(500); }
  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  bool wantsEsc() override { return _page != PAGE_MAIN; }
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  enum Page {
    PAGE_MAIN = 0,
    PAGE_GPX_LIST
  };

  static const int GPX_LIST_MAX = 8;
  static const int GPX_NAME_MAX = 48;
  static const int GPX_ITEMS_PER_PAGE = 5;

  void _drawInactive();
  void _drawActive();
  void _drawGpxList();
  void _refreshGpxFiles();
  void _loadSelectedGpx();

  Page _page = PAGE_MAIN;
  char _gpxFiles[GPX_LIST_MAX][GPX_NAME_MAX] = {};
  int _gpxCount = 0;
  int _selectedIdx = 0;
  int _scrollOffset = 0;
  bool _listLoaded = false;
  char _feedback[48] = "";
};

#endif // FN_BACKTRACK_H
