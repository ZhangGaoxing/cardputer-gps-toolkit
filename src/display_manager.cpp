/**
 * display_manager.cpp — 显示管理器实现
 */
#include "display_manager.h"

DisplayManager& DisplayManager::instance() {
  static DisplayManager dm;
  return dm;
}

DisplayManager::~DisplayManager() {
  if (_frameBuf) {
    _frameBuf->deleteSprite();
    delete _frameBuf;
    _frameBuf = nullptr;
  }
}

void DisplayManager::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);
  _frameBuf = new M5Canvas(&M5Cardputer.Display);
  _frameBuf->createSprite(SCREEN_W, SCREEN_H);
}

void DisplayManager::clearScreen(uint16_t color) {
  _frameBuf->fillScreen(color);
}

void DisplayManager::commit() {
  _frameBuf->pushSprite(0, 0);
}
