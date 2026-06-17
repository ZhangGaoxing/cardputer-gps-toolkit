#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "emergency_info.h"

enum class RadioState : uint8_t {
  Disabled = 0,
  Initializing,
  Ready,
  Tx,
  Rx,
  Error
};

enum class RadioPacketKind : uint8_t {
  Unknown = 0,
  Position,
  SOS,
  Ack
};

enum class RadioFixStatus : uint8_t {
  Unavailable = 0,
  Reliable,
  LastKnown
};

struct RadioPacket {
  RadioPacketKind kind = RadioPacketKind::Unknown;
  char deviceId[8] = "";
  uint16_t sequence = 0;
  RadioFixStatus fix = RadioFixStatus::Unavailable;
  bool hasPosition = false;
  float lat = NAN;
  float lon = NAN;
  float eleM = NAN;
  uint32_t ageSec = UINT32_MAX;
  int batteryPct = -1;
  float hdop = 99.9f;
  int satellites = 0;
  char timestamp[24] = "";
  char message[32] = "";
  int rssi = 0;
  float snr = 0.0f;
  char raw[RADIO_MAX_PACKET_LEN + 1] = "";
};

class RadioManager {
public:
  static RadioManager& instance();

  bool begin();
  bool update();
  bool isAvailable() const { return _available; }
  RadioState state() const { return _state; }
  const char* stateText() const;
  const char* deviceId() const { return _deviceId; }

  bool sendPosition();
  bool sendSOS(const EmergencySnapshot& snapshot, const char* message = "NEED_HELP");
  bool startSosBroadcast(const char* message = "NEED_HELP");
  bool sosBroadcastActive() const { return _sosRemaining > 0; }
  uint8_t sosRemaining() const { return _sosRemaining; }
  bool sendPacket(const char* packet);

  bool hasReceivedPacket() const { return _hasRxPacket; }
  bool readReceivedPacket(RadioPacket& out);
  bool getLastError(char* out, size_t outSize) const;
  const char* lastTxStatus() const { return _lastTxStatus; }
  int lastRSSI() const { return _lastRSSI; }
  float lastSNR() const { return _lastSNR; }

  static bool parsePacket(const char* text, RadioPacket& out, char* errorOut, size_t errorSize);
  static const char* fixStatusText(RadioFixStatus status);
  static const char* packetKindText(RadioPacketKind kind);

private:
  RadioManager() = default;
  RadioManager(const RadioManager&) = delete;
  RadioManager& operator=(const RadioManager&) = delete;

  void _setState(RadioState state);
  void _setError(const char* error);
  void _setTxStatus(const char* status);
  void _makeDeviceId();
  bool _buildPositionPacket(char* out, size_t outSize, bool sos,
                            const EmergencySnapshot* sosSnapshot,
                            const char* message);
  bool _hardwareBegin();
  bool _hardwareSend(const char* packet);
  bool _hardwareReceive();
  unsigned long _broadcastInterval() const;

  RadioState _state = RadioState::Disabled;
  bool _available = false;
  bool _begun = false;
  char _deviceId[8] = "";
  uint16_t _sequence = 0;
  unsigned long _lastBroadcastMs = 0;
  char _lastError[64] = "";
  char _lastTxStatus[48] = "";
  bool _txInProgress = false;
  int _lastRSSI = 0;
  float _lastSNR = 0.0f;
  RadioPacket _rxPacket;
  bool _hasRxPacket = false;

  char _sosMessage[32] = "";
  uint8_t _sosRemaining = 0;
  unsigned long _nextSosMs = 0;
};

#endif // RADIO_MANAGER_H
