#include "peer_manager.h"

#include <math.h>
#include <string.h>

namespace {

bool sequenceIsNewer(uint16_t incoming, uint16_t previous) {
  if (incoming == previous) return false;
  return (uint16_t)(incoming - previous) < 32768U;
}

bool packetPositionTooOld(const RadioPacket& packet) {
  if (!packet.hasPosition) return false;
  if (packet.fix != RadioFixStatus::LastKnown) return false;
  if (packet.ageSec == UINT32_MAX) return true;
  return packet.ageSec > (RADIO_LAST_KNOWN_MAX_AGE_MS / 1000UL);
}

} // namespace

PeerManager& PeerManager::instance() {
  static PeerManager pm;
  return pm;
}

void PeerManager::begin() {
  memset(_peers, 0, sizeof(_peers));
  _count = 0;
  _changeCounter = 0;
  _setError("");
}

void PeerManager::update() {
  _compactLost(millis());
}

PeerUpdateResult PeerManager::updateFromPacket(const RadioPacket& packet) {
  if (packet.deviceId[0] == '\0') {
    _setError("Empty device id");
    return PeerUpdateResult::Invalid;
  }
  if (strcmp(packet.deviceId, RadioManager::instance().deviceId()) == 0) {
    return PeerUpdateResult::Self;
  }

  int idx = _findById(packet.deviceId);
  if (idx >= 0) {
    RemoteDevice& peer = _peers[idx];
    if (peer.hasSequence && !sequenceIsNewer(packet.sequence, peer.lastSequence)) {
      _setError("Duplicate sequence");
      return PeerUpdateResult::Duplicate;
    }
  } else {
    idx = _selectSlot();
    if (idx < 0) {
      _setError("Peer list full");
      return PeerUpdateResult::Full;
    }
    if (!_peers[idx].used) {
      _count++;
    }
    memset(&_peers[idx], 0, sizeof(RemoteDevice));
    _peers[idx].used = true;
    strncpy(_peers[idx].deviceId, packet.deviceId, sizeof(_peers[idx].deviceId) - 1);
    _peers[idx].deviceId[sizeof(_peers[idx].deviceId) - 1] = '\0';
  }

  RemoteDevice& peer = _peers[idx];
  bool hasUsablePosition = packet.hasPosition && !packetPositionTooOld(packet);
  peer.fix = packet.fix;
  peer.hasPosition = hasUsablePosition;
  if (hasUsablePosition) {
    peer.lat = packet.lat;
    peer.lon = packet.lon;
    peer.eleM = packet.eleM;
  } else {
    peer.lat = NAN;
    peer.lon = NAN;
    peer.eleM = NAN;
  }
  peer.hdop = packet.hdop;
  peer.satellites = packet.satellites;
  peer.batteryPct = packet.batteryPct;
  peer.positionAgeSec = packet.ageSec;
  peer.lastRSSI = packet.rssi;
  peer.lastSNR = packet.snr;
  peer.lastSeenMs = millis();
  peer.sosActive = packet.kind == RadioPacketKind::SOS;
  peer.lastSequence = packet.sequence;
  peer.hasSequence = true;
  peer.packetCount++;
  _changeCounter++;
  _setError("");
  return PeerUpdateResult::Accepted;
}

const RemoteDevice* PeerManager::getByIndex(size_t index) const {
  size_t seen = 0;
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (!_peers[i].used) continue;
    if (seen == index) return &_peers[i];
    seen++;
  }
  return nullptr;
}

bool PeerManager::isStale(const RemoteDevice& peer, unsigned long now) const {
  if (!peer.used || peer.lastSeenMs == 0) return true;
  return now - peer.lastSeenMs >= PEER_STALE_TIMEOUT_MS;
}

bool PeerManager::isLost(const RemoteDevice& peer, unsigned long now) const {
  if (!peer.used || peer.lastSeenMs == 0) return false;
  return now - peer.lastSeenMs >= PEER_LOST_TIMEOUT_MS;
}

int PeerManager::_findById(const char* deviceId) const {
  if (!deviceId || deviceId[0] == '\0') return -1;
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (_peers[i].used && strcmp(_peers[i].deviceId, deviceId) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int PeerManager::_selectSlot() {
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (!_peers[i].used) return (int)i;
  }

  unsigned long now = millis();
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (isLost(_peers[i], now)) {
      return (int)i;
    }
  }

  int oldest = -1;
  unsigned long oldestSeen = ULONG_MAX;
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (_peers[i].used && isStale(_peers[i], now) && _peers[i].lastSeenMs < oldestSeen) {
      oldestSeen = _peers[i].lastSeenMs;
      oldest = (int)i;
    }
  }
  if (oldest >= 0) {
    return oldest;
  }
  return -1;
}

void PeerManager::_setError(const char* error) {
  if (!error) error = "";
  strncpy(_lastError, error, sizeof(_lastError) - 1);
  _lastError[sizeof(_lastError) - 1] = '\0';
}

void PeerManager::_compactLost(unsigned long now) {
  bool changed = false;
  for (size_t i = 0; i < PEER_MAX_COUNT; i++) {
    if (_peers[i].used && isLost(_peers[i], now)) {
      memset(&_peers[i], 0, sizeof(RemoteDevice));
      if (_count > 0) _count--;
      changed = true;
    }
  }
  if (changed) _changeCounter++;
}
