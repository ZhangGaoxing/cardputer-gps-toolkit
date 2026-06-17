#include "fn_peers.h"

#include "../display_manager.h"
#include "../geo_math.h"
#include "../gps_manager.h"
#include "../peer_manager.h"
#include "../radio_manager.h"
#include "../ui_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void formatAge(char* out, size_t outSize, unsigned long ageMs) {
  if (!out || outSize == 0) return;
  unsigned long sec = ageMs / 1000UL;
  if (sec < 60UL) {
    snprintf(out, outSize, "%lus", sec);
  } else if (sec < 3600UL) {
    snprintf(out, outSize, "%lum", sec / 60UL);
  } else {
    snprintf(out, outSize, "%luh", sec / 3600UL);
  }
}

void formatDistance(char* out, size_t outSize, float meters) {
  if (!out || outSize == 0) return;
  if (!isfinite(meters)) {
    snprintf(out, outSize, "--");
  } else if (meters < 1000.0f) {
    snprintf(out, outSize, "%.0fm", meters);
  } else if (meters < 10000.0f) {
    snprintf(out, outSize, "%.2fkm", meters / 1000.0f);
  } else {
    snprintf(out, outSize, "%.1fkm", meters / 1000.0f);
  }
}

uint16_t peerColor(const RemoteDevice& peer, bool stale) {
  if (peer.sosActive) return TFT_RED;
  if (stale) return TFT_DARKGREY;
  if (peer.fix == RadioFixStatus::Reliable) return TFT_GREEN;
  if (peer.fix == RadioFixStatus::LastKnown) return TFT_YELLOW;
  return TFT_LIGHTGREY;
}

} // namespace

void FnPeers::onEnter() {
  _scroll = 0;
}

void FnPeers::onUpdate(bool force) {
  (void)force;
  M5Canvas& cv = DisplayManager::instance().canvas();
  RadioManager& radio = RadioManager::instance();
  PeerManager& peers = PeerManager::instance();
  GPSManager& gps = GPSManager::instance();
  unsigned long now = millis();

  cv.fillScreen(TFT_BLACK);
  cv.setTextSize(1);

  cv.setTextColor(TFT_CYAN);
  cv.setCursor(4, 2);
  cv.print("LoRa / Mesh Peers");

  uint16_t stateColor = radio.isAvailable() ? TFT_GREEN : TFT_ORANGE;
  cv.setTextColor(stateColor);
  cv.setCursor(4, 16);
  cv.printf("%s  ID %s", radio.stateText(), radio.deviceId());

  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 28);
  cv.printf("RSSI %d  SNR %.1f  %s", radio.lastRSSI(), radio.lastSNR(), radio.lastTxStatus());

  if (!radio.isAvailable()) {
    char err[64];
    radio.getLastError(err, sizeof(err));
    cv.setTextColor(TFT_ORANGE);
    cv.setCursor(4, 40);
    cv.printf("Radio: %.34s", err[0] ? err : "Unavailable");
  }

  cv.drawLine(0, 52, SCREEN_W - 1, 52, UI_DIM);

  size_t total = peers.count();
  if (total == 0) {
    cv.setTextColor(TFT_DARKGREY);
    cv.setCursor(72, 74);
    cv.print("No peers");
    cv.setCursor(32, 88);
    cv.print("Waiting for GPS1 packets");
  } else {
    int visibleRows = 5;
    if (_scroll > (int)total - visibleRows) _scroll = max(0, (int)total - visibleRows);
    if (_scroll < 0) _scroll = 0;

    bool canMeasure = gps.hasReliableFix();
    float ownLat = canMeasure ? gps.latitude() : NAN;
    float ownLon = canMeasure ? gps.longitude() : NAN;

    for (int row = 0; row < visibleRows; row++) {
      int idx = _scroll + row;
      const RemoteDevice* peer = peers.getByIndex(idx);
      if (!peer) break;
      bool stale = peers.isStale(*peer, now);
      int y = 56 + row * 14;
      uint16_t col = peerColor(*peer, stale);

      char ageBuf[12];
      formatAge(ageBuf, sizeof(ageBuf), now - peer->lastSeenMs);

      char distBuf[16] = "--";
      const char* bearingText = "--";
      if (canMeasure && peer->hasPosition) {
        float distM = geoDistanceKm(ownLat, ownLon, peer->lat, peer->lon) * 1000.0f;
        float bearing = geoBearingDeg(ownLat, ownLon, peer->lat, peer->lon);
        formatDistance(distBuf, sizeof(distBuf), distM);
        bearingText = cardinalFromHeading(bearing);
      }

      cv.setTextColor(col);
      cv.setCursor(4, y);
      cv.printf("%s%s", peer->sosActive ? "!" : " ", peer->deviceId);

      cv.setTextColor(stale ? TFT_DARKGREY : TFT_WHITE);
      cv.setCursor(58, y);
      cv.printf("%s %s", distBuf, bearingText);

      cv.setTextColor(col);
      cv.setCursor(130, y);
      if (peer->batteryPct >= 0) cv.printf("B%d", peer->batteryPct);
      else cv.print("B--");

      cv.setTextColor(stale ? TFT_DARKGREY : TFT_LIGHTGREY);
      cv.setCursor(162, y);
      cv.printf("%s R%d", ageBuf, peer->lastRSSI);

      if (stale) {
        cv.setTextColor(TFT_ORANGE);
        cv.setCursor(220, y);
        cv.print("S");
      }
    }
  }

  cv.drawLine(0, SCREEN_H - 12, SCREEN_W - 1, SCREEN_H - 12, UI_DIM);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, SCREEN_H - 9);
  cv.print("[;]/[.]Scroll  [`]Back");
  if (total > 0) {
    char page[16];
    snprintf(page, sizeof(page), "%d/%u", _scroll + 1, (unsigned)total);
    int w = strlen(page) * 6;
    cv.setCursor(SCREEN_W - w - 4, SCREEN_H - 9);
    cv.print(page);
  }
}

bool FnPeers::onKeyEvent(const KeyEvent& event) {
  if (!event.pressed && !event.held) return false;
  size_t total = PeerManager::instance().count();
  if (event.key == ';') {
    if (_scroll > 0) _scroll--;
    return true;
  }
  if (event.key == '.') {
    if (_scroll + 1 < (int)total) _scroll++;
    return true;
  }
  return false;
}

void FnPeers::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2;
  int cy = y + size / 2;
  int r = size / 2 - 4;
  cv.drawCircle(cx, cy, r, color);
  cv.drawCircle(cx, cy, r / 2, color);
  cv.fillCircle(cx, cy, 3, color);
  cv.drawLine(cx, cy - r, cx, cy - r / 2, color);
  cv.drawLine(cx, cy + r / 2, cx, cy + r, color);
  cv.drawLine(cx - r, cy, cx - r / 2, cy, color);
  cv.drawLine(cx + r / 2, cy, cx + r, cy, color);
}
