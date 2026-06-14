/**
 * display_manager.cpp — 显示管理器实现
 */
#include "display_manager.h"

namespace {
const uint8_t kBrightnessValues[BRIGHTNESS_LEVEL_COUNT] = {48, 128, 255};
const unsigned long kSleepTimeoutValues[SLEEP_TIMEOUT_COUNT] = {
  60000UL,
  300000UL,
  600000UL,
  0UL
};
const char* const kBrightnessLabels[BRIGHTNESS_LEVEL_COUNT] = {"Low", "Medium", "High"};
const char* const kSleepTimeoutLabels[SLEEP_TIMEOUT_COUNT] = {"1 min", "5 min", "10 min", "Always"};
}

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
  _applyBrightness();
  _frameBuf = new M5Canvas(&M5Cardputer.Display);
  _frameBuf->createSprite(SCREEN_W, SCREEN_H);
}

void DisplayManager::clearScreen(uint16_t color) {
  _frameBuf->fillScreen(color);
}

void DisplayManager::commit() {
  _frameBuf->pushSprite(0, 0);
}

void DisplayManager::setBrightnessLevel(uint8_t level) {
  if (level >= BRIGHTNESS_LEVEL_COUNT) {
    level = BRIGHTNESS_LEVEL_COUNT - 1;
  }
  _brightnessLevel = level;
  _screenSleeping = false;
  _applyBrightness();
}

uint8_t DisplayManager::brightnessValue() const {
  return kBrightnessValues[_brightnessLevel];
}

void DisplayManager::setSleepTimeoutIndex(uint8_t index) {
  if (index >= SLEEP_TIMEOUT_COUNT) {
    index = SLEEP_TIMEOUT_COUNT - 1;
  }
  _sleepTimeoutIndex = index;
}

unsigned long DisplayManager::sleepTimeoutMs() const {
  return kSleepTimeoutValues[_sleepTimeoutIndex];
}

void DisplayManager::updatePowerState(unsigned long now, unsigned long lastActivityMs) {
  unsigned long timeout = sleepTimeoutMs();
  bool shouldSleep = (timeout > 0) && (now - lastActivityMs >= timeout);

  if (shouldSleep) {
    if (!_screenSleeping) {
      _screenSleeping = true;
      M5Cardputer.Display.setBrightness(0);
    }
    return;
  }

  if (_screenSleeping) {
    _screenSleeping = false;
    _applyBrightness();
  }
}

const char* DisplayManager::brightnessLabel() const {
  return kBrightnessLabels[_brightnessLevel];
}

const char* DisplayManager::sleepTimeoutLabel() const {
  return kSleepTimeoutLabels[_sleepTimeoutIndex];
}

void DisplayManager::_applyBrightness() {
  M5Cardputer.Display.setBrightness(kBrightnessValues[_brightnessLevel]);
}
