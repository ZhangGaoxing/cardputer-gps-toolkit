#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "radio_manager.h"

struct RemoteDevice {
  bool used = false;
  char deviceId[8] = "";
  bool hasPosition = false;
  float lat = NAN;
  float lon = NAN;
  float eleM = NAN;
  RadioFixStatus fix = RadioFixStatus::Unavailable;
  float hdop = 99.9f;
  int satellites = 0;
  int batteryPct = -1;
  unsigned long lastSeenMs = 0;
  uint32_t positionAgeSec = UINT32_MAX;
  int lastRSSI = 0;
  float lastSNR = 0.0f;
  bool sosActive = false;
  uint16_t lastSequence = 0;
  bool hasSequence = false;
  uint32_t packetCount = 0;
};

enum class PeerUpdateResult : uint8_t {
  Accepted = 0,
  Duplicate,
  Invalid,
  Full,
  Self
};

class PeerManager {
public:
  static PeerManager& instance();

  void begin();
  void update();
  PeerUpdateResult updateFromPacket(const RadioPacket& packet);
  size_t count() const { return _count; }
  const RemoteDevice* getByIndex(size_t index) const;
  bool isStale(const RemoteDevice& peer, unsigned long now = millis()) const;
  bool isLost(const RemoteDevice& peer, unsigned long now = millis()) const;
  uint32_t changeCounter() const { return _changeCounter; }
  const char* lastError() const { return _lastError; }

private:
  PeerManager() = default;
  PeerManager(const PeerManager&) = delete;
  PeerManager& operator=(const PeerManager&) = delete;

  int _findById(const char* deviceId) const;
  int _selectSlot();
  void _setError(const char* error);
  void _compactLost(unsigned long now);

  RemoteDevice _peers[PEER_MAX_COUNT];
  size_t _count = 0;
  uint32_t _changeCounter = 0;
  char _lastError[48] = "";
};

#endif // PEER_MANAGER_H
