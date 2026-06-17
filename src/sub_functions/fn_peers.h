#ifndef FN_PEERS_H
#define FN_PEERS_H

#include "sub_function.h"

class FnPeers : public SubFunction {
public:
  FnPeers() : SubFunction("Peers", ICON_PEERS) { setUpdateInterval(500); }

  void onEnter() override;
  void onUpdate(bool force) override;
  bool onKeyEvent(const KeyEvent& event) override;
  void drawIcon(int x, int y, int size, uint16_t color) override;

private:
  int _scroll = 0;
};

#endif // FN_PEERS_H
