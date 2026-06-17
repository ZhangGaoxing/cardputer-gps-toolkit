#include "radio_manager.h"

#include "battery_manager.h"
#include "gps_manager.h"
#include "navigation_manager.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if RADIO_USE_RADIOLIB
#include <RadioLib.h>
#include <SPI.h>
static SX1262 gRadio = new Module(RADIO_NSS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
static volatile bool gRadioRxFlag = false;
static volatile bool gRadioTxFlag = false;

static void IRAM_ATTR onRadioPacketReceived() {
  gRadioRxFlag = true;
}

static void IRAM_ATTR onRadioPacketSent() {
  gRadioTxFlag = true;
}
#endif

namespace {

bool validCoordinate(float lat, float lon) {
  return isfinite(lat) && isfinite(lon) &&
         lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f;
}

void copyBounded(char* out, size_t outSize, const char* value) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!value) return;
  strncpy(out, value, outSize - 1);
  out[outSize - 1] = '\0';
}

void sanitizeToken(char* text) {
  if (!text) return;
  for (size_t i = 0; text[i]; i++) {
    char c = text[i];
    if (c == '|' || c == '=' || c == '\r' || c == '\n') {
      text[i] = '_';
    }
  }
}

bool parseUint16(const char* text, uint16_t& out) {
  if (!text || !*text) return false;
  char* endPtr = nullptr;
  unsigned long value = strtoul(text, &endPtr, 10);
  if (endPtr == text || *endPtr != '\0' || value > 65535UL) return false;
  out = (uint16_t)value;
  return true;
}

bool parseInt(const char* text, int& out) {
  if (!text || !*text) return false;
  char* endPtr = nullptr;
  long value = strtol(text, &endPtr, 10);
  if (endPtr == text || *endPtr != '\0') return false;
  out = (int)value;
  return true;
}

bool parseFloat(const char* text, float& out) {
  if (!text || !*text) return false;
  char* endPtr = nullptr;
  float value = strtof(text, &endPtr);
  if (endPtr == text || *endPtr != '\0' || !isfinite(value)) return false;
  out = value;
  return true;
}

RadioFixStatus parseFix(const char* value) {
  if (!value) return RadioFixStatus::Unavailable;
  if (strcmp(value, "REL") == 0) return RadioFixStatus::Reliable;
  if (strcmp(value, "LAST") == 0) return RadioFixStatus::LastKnown;
  return RadioFixStatus::Unavailable;
}

const char* fixCode(RadioFixStatus status) {
  switch (status) {
    case RadioFixStatus::Reliable: return "REL";
    case RadioFixStatus::LastKnown: return "LAST";
    default: return "UNAV";
  }
}

void formatGpsTime(char* out, size_t outSize, GPSManager& gps) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (gps.dateValid() && gps.timeValid()) {
    snprintf(out, outSize, "%04d%02d%02dT%02d%02d%02dZ",
             gps.utcYear(), gps.utcMonth(), gps.utcDay(),
             gps.utcHour(), gps.utcMinute(), gps.utcSecond());
  }
}

void setErr(char* errorOut, size_t errorSize, const char* msg) {
  if (!errorOut || errorSize == 0) return;
  copyBounded(errorOut, errorSize, msg);
}

} // namespace

RadioManager& RadioManager::instance() {
  static RadioManager rm;
  return rm;
}

bool RadioManager::begin() {
  if (_begun) return _available;
  _begun = true;
  _makeDeviceId();

#if !RADIO_ENABLED
  _setState(RadioState::Disabled);
  _setError("Radio disabled by config");
  return false;
#else
  _setState(RadioState::Initializing);
  bool ok = _hardwareBegin();
  if (ok) {
    _available = true;
    _setState(RadioState::Ready);
    _setError("");
    _setTxStatus("Radio ready");
  } else {
    _available = false;
    _setState(RadioState::Error);
    if (_lastError[0] == '\0') _setError("Radio init failed");
  }
  return _available;
#endif
}

bool RadioManager::update() {
  if (!_begun) begin();
  if (!_available) return false;

  bool didWork = _hardwareReceive();
  unsigned long now = millis();

  if (_sosRemaining > 0 && now >= _nextSosMs) {
    EmergencySnapshot snapshot;
    EmergencyInfo::buildSnapshot(snapshot);
    bool sent = sendSOS(snapshot, _sosMessage[0] ? _sosMessage : "NEED_HELP");
    if (sent) {
      _sosRemaining--;
      _nextSosMs = now + SOS_LORA_REPEAT_INTERVAL_MS;
    } else if (strstr(_lastError, "busy") != nullptr) {
      _nextSosMs = now + 100UL;
    } else {
      if (_sosRemaining > 0) _sosRemaining--;
      _nextSosMs = now + SOS_LORA_REPEAT_INTERVAL_MS;
    }
    didWork = true;
  }

  if (RADIO_POSITION_BROADCAST_ENABLED &&
      (_lastBroadcastMs == 0 || now - _lastBroadcastMs >= _broadcastInterval())) {
    if (sendPosition()) {
      _lastBroadcastMs = now;
      didWork = true;
    } else if (strstr(_lastError, "No position") != nullptr) {
      _lastBroadcastMs = now;
    }
  }

  return didWork;
}

bool RadioManager::sendPosition() {
  char packet[RADIO_MAX_PACKET_LEN + 1];
  if (!_buildPositionPacket(packet, sizeof(packet), false, nullptr, nullptr)) {
    return false;
  }
  return sendPacket(packet);
}

bool RadioManager::sendSOS(const EmergencySnapshot& snapshot, const char* message) {
  char packet[RADIO_MAX_PACKET_LEN + 1];
  if (!_buildPositionPacket(packet, sizeof(packet), true, &snapshot, message)) {
    return false;
  }
  return sendPacket(packet);
}

bool RadioManager::startSosBroadcast(const char* message) {
  if (!_available) {
    _setTxStatus("LoRa unavailable");
    if (_lastError[0] == '\0') _setError("LoRa unavailable");
    return false;
  }
  copyBounded(_sosMessage, sizeof(_sosMessage), message ? message : "NEED_HELP");
  sanitizeToken(_sosMessage);
  _sosRemaining = SOS_LORA_REPEAT_COUNT;
  _nextSosMs = 0;
  _setTxStatus("SOS queued");
  return true;
}

bool RadioManager::sendPacket(const char* packet) {
  if (!packet || packet[0] == '\0') {
    _setError("Empty packet");
    _setTxStatus("TX empty");
    return false;
  }
  size_t len = strlen(packet);
  if (len > RADIO_MAX_PACKET_LEN) {
    _setError("Packet too long");
    _setTxStatus("TX too long");
    return false;
  }
  if (!_available) {
    _setTxStatus("LoRa unavailable");
    if (_lastError[0] == '\0') _setError("LoRa unavailable");
    return false;
  }
  if (_txInProgress || _state == RadioState::Tx) {
    _setError("Radio busy");
    _setTxStatus("TX busy");
    return false;
  }

  _setState(RadioState::Tx);
  bool ok = _hardwareSend(packet);
  if (ok) {
    _txInProgress = true;
    _setTxStatus("TX started");
  } else {
    _setState(RadioState::Error);
    _setTxStatus("TX failed");
  }
  return ok;
}

bool RadioManager::readReceivedPacket(RadioPacket& out) {
  if (!_hasRxPacket) return false;
  out = _rxPacket;
  _hasRxPacket = false;
  return true;
}

bool RadioManager::getLastError(char* out, size_t outSize) const {
  if (!out || outSize == 0) return false;
  copyBounded(out, outSize, _lastError);
  return _lastError[0] != '\0';
}

const char* RadioManager::stateText() const {
  switch (_state) {
    case RadioState::Disabled: return "Disabled";
    case RadioState::Initializing: return "Initializing";
    case RadioState::Ready: return "Ready";
    case RadioState::Tx: return "Tx";
    case RadioState::Rx: return "Rx";
    case RadioState::Error: return "Error";
    default: return "Unknown";
  }
}

const char* RadioManager::fixStatusText(RadioFixStatus status) {
  switch (status) {
    case RadioFixStatus::Reliable: return "Reliable";
    case RadioFixStatus::LastKnown: return "Last";
    default: return "Unavailable";
  }
}

const char* RadioManager::packetKindText(RadioPacketKind kind) {
  switch (kind) {
    case RadioPacketKind::Position: return "POS";
    case RadioPacketKind::SOS: return "SOS";
    case RadioPacketKind::Ack: return "ACK";
    default: return "UNK";
  }
}

bool RadioManager::parsePacket(const char* text, RadioPacket& out, char* errorOut, size_t errorSize) {
  out = RadioPacket();
  if (!text || text[0] == '\0') {
    setErr(errorOut, errorSize, "Empty packet");
    return false;
  }
  size_t len = strlen(text);
  if (len > RADIO_MAX_PACKET_LEN) {
    setErr(errorOut, errorSize, "Packet too long");
    return false;
  }
  copyBounded(out.raw, sizeof(out.raw), text);

  char work[RADIO_MAX_PACKET_LEN + 1];
  copyBounded(work, sizeof(work), text);
  char* save = nullptr;
  char* token = strtok_r(work, "|", &save);
  if (!token || strcmp(token, "GPS1") != 0) {
    setErr(errorOut, errorSize, "Unknown protocol");
    return false;
  }

  token = strtok_r(nullptr, "|", &save);
  if (!token) {
    setErr(errorOut, errorSize, "Missing packet type");
    return false;
  }
  if (strcmp(token, "POS") == 0) out.kind = RadioPacketKind::Position;
  else if (strcmp(token, "SOS") == 0) out.kind = RadioPacketKind::SOS;
  else if (strcmp(token, "ACK") == 0) out.kind = RadioPacketKind::Ack;
  else {
    setErr(errorOut, errorSize, "Unknown packet type");
    return false;
  }

  bool sawSeq = false;
  bool sawFix = false;
  while ((token = strtok_r(nullptr, "|", &save)) != nullptr) {
    char* eq = strchr(token, '=');
    if (!eq || eq == token) continue;
    *eq = '\0';
    const char* key = token;
    const char* value = eq + 1;

    if (strcmp(key, "DEV") == 0) {
      copyBounded(out.deviceId, sizeof(out.deviceId), value);
    } else if (strcmp(key, "SEQ") == 0) {
      sawSeq = parseUint16(value, out.sequence);
    } else if (strcmp(key, "LAT") == 0) {
      parseFloat(value, out.lat);
    } else if (strcmp(key, "LON") == 0) {
      parseFloat(value, out.lon);
    } else if (strcmp(key, "ELE") == 0) {
      parseFloat(value, out.eleM);
    } else if (strcmp(key, "FIX") == 0) {
      out.fix = parseFix(value);
      sawFix = true;
    } else if (strcmp(key, "AGE") == 0) {
      int age = 0;
      if (parseInt(value, age) && age >= 0) out.ageSec = (uint32_t)age;
    } else if (strcmp(key, "BAT") == 0) {
      parseInt(value, out.batteryPct);
    } else if (strcmp(key, "HDOP") == 0) {
      parseFloat(value, out.hdop);
    } else if (strcmp(key, "SAT") == 0) {
      parseInt(value, out.satellites);
    } else if (strcmp(key, "T") == 0) {
      copyBounded(out.timestamp, sizeof(out.timestamp), value);
    } else if (strcmp(key, "MSG") == 0) {
      copyBounded(out.message, sizeof(out.message), value);
    }
  }

  if (out.deviceId[0] == '\0') {
    setErr(errorOut, errorSize, "Missing device id");
    return false;
  }
  if (!sawSeq) {
    setErr(errorOut, errorSize, "Missing sequence");
    return false;
  }
  if (!sawFix) {
    setErr(errorOut, errorSize, "Missing fix");
    return false;
  }
  out.hasPosition = validCoordinate(out.lat, out.lon) &&
                    out.fix != RadioFixStatus::Unavailable;
  if ((out.fix == RadioFixStatus::Reliable || out.fix == RadioFixStatus::LastKnown) &&
      !out.hasPosition) {
    setErr(errorOut, errorSize, "Invalid coordinates");
    return false;
  }
  return true;
}

void RadioManager::_setState(RadioState state) {
  _state = state;
}

void RadioManager::_setError(const char* error) {
  copyBounded(_lastError, sizeof(_lastError), error ? error : "");
}

void RadioManager::_setTxStatus(const char* status) {
  copyBounded(_lastTxStatus, sizeof(_lastTxStatus), status ? status : "");
}

void RadioManager::_makeDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t shortId = (uint32_t)(mac & 0xFFFFFFUL);
  snprintf(_deviceId, sizeof(_deviceId), "%06lX", (unsigned long)shortId);
}

bool RadioManager::_buildPositionPacket(char* out, size_t outSize, bool sos,
                                        const EmergencySnapshot* sosSnapshot,
                                        const char* message) {
  if (!out || outSize == 0) return false;
  out[0] = '\0';

  RadioFixStatus fix = RadioFixStatus::Unavailable;
  float lat = NAN;
  float lon = NAN;
  float ele = NAN;
  float hdop = 99.9f;
  int sats = 0;
  uint32_t ageSec = UINT32_MAX;
  char timeBuf[24] = "";
  GPSManager& gps = GPSManager::instance();

  if (sos && sosSnapshot) {
    if (sosSnapshot->positionSource == EMERGENCY_POS_CURRENT) fix = RadioFixStatus::Reliable;
    else if (sosSnapshot->positionSource == EMERGENCY_POS_LAST_KNOWN &&
             !sosSnapshot->positionExpired) fix = RadioFixStatus::LastKnown;
    if (sosSnapshot->positionAvailable) {
      if (fix != RadioFixStatus::Unavailable) {
        lat = sosSnapshot->lat;
        lon = sosSnapshot->lon;
        ele = sosSnapshot->altitudeValid ? sosSnapshot->altM : NAN;
      }
      hdop = sosSnapshot->hdop;
      sats = sosSnapshot->satellites;
      if (fix == RadioFixStatus::Reliable) ageSec = 0;
      else if (gps.hasLastReliableFix()) ageSec = gps.lastReliableFixAgeMs() / 1000UL;
    }
    copyBounded(timeBuf, sizeof(timeBuf), sosSnapshot->payloadTimeText);
  } else if (gps.hasReliableFix()) {
    fix = RadioFixStatus::Reliable;
    lat = gps.latitude();
    lon = gps.longitude();
    ele = gps.altitudeValid() ? gps.altitude() : NAN;
    hdop = gps.hdop();
    sats = gps.satellitesUsed();
    ageSec = gps.fixAgeMs() / 1000UL;
    formatGpsTime(timeBuf, sizeof(timeBuf), gps);
  } else if (RADIO_ALLOW_LAST_KNOWN_BROADCAST && gps.hasLastReliableFix() &&
             gps.lastReliableFixAgeMs() <= RADIO_LAST_KNOWN_MAX_AGE_MS) {
    const ReliableFixSnapshot& last = gps.lastReliableFix();
    fix = RadioFixStatus::LastKnown;
    lat = last.lat;
    lon = last.lon;
    ele = last.altValid ? last.altM : NAN;
    hdop = last.hdop;
    sats = last.satellitesUsed;
    ageSec = gps.lastReliableFixAgeMs() / 1000UL;
    if (last.dateValid && last.timeValid) {
      snprintf(timeBuf, sizeof(timeBuf), "%04d%02d%02dT%02d%02d%02dZ",
               last.year, last.month, last.day,
               last.hour, last.minute, last.second);
    }
  }

  if (fix != RadioFixStatus::Unavailable && !validCoordinate(lat, lon)) {
    _setError("Invalid position");
    return false;
  }
  if (!sos && fix == RadioFixStatus::Unavailable) {
    _setError("No position to broadcast");
    return false;
  }

  uint16_t seq = ++_sequence;
  int battery = BatteryManager::instance().percentage();
  const char* kind = sos ? "SOS" : "POS";
  uint32_t cappedAge = ageSec > 999999UL ? 999999UL : ageSec;
  int age = (ageSec == UINT32_MAX) ? -1 : (int)cappedAge;
  char msgBuf[32] = "";
  if (message && message[0]) {
    copyBounded(msgBuf, sizeof(msgBuf), message);
    sanitizeToken(msgBuf);
  }

  int written = 0;
  if (fix == RadioFixStatus::Unavailable) {
    written = snprintf(out, outSize,
                       "GPS1|%s|DEV=%s|SEQ=%u|FIX=UNAV|AGE=%d|BAT=%d|HDOP=%.1f|SAT=%d",
                       kind, _deviceId, (unsigned)seq, age, battery, hdop, sats);
  } else {
    written = snprintf(out, outSize,
                       "GPS1|%s|DEV=%s|SEQ=%u|LAT=%.6f|LON=%.6f|ELE=%.0f|FIX=%s|AGE=%d|BAT=%d|HDOP=%.1f|SAT=%d",
                       kind, _deviceId, (unsigned)seq, lat, lon,
                       isfinite(ele) ? ele : 0.0f,
                       fixCode(fix), age, battery, hdop, sats);
  }
  if (written < 0 || (size_t)written >= outSize) {
    _setError("Packet build overflow");
    return false;
  }

  size_t used = strlen(out);
  if (timeBuf[0] != '\0' && used + strlen(timeBuf) + 4 < outSize) {
    snprintf(out + used, outSize - used, "|T=%s", timeBuf);
  }
  used = strlen(out);
  if (sos && msgBuf[0] != '\0' && used + strlen(msgBuf) + 6 < outSize) {
    snprintf(out + used, outSize - used, "|MSG=%s", msgBuf);
  }
  if (strlen(out) > RADIO_MAX_PACKET_LEN) {
    _setError("Packet too long");
    return false;
  }
  return true;
}

bool RadioManager::_hardwareBegin() {
#if RADIO_USE_RADIOLIB
  if (RADIO_DIO1_PIN < 0 || RADIO_RST_PIN < 0 || RADIO_BUSY_PIN < 0) {
    _setError("Radio pins unset");
    return false;
  }
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(RADIO_NSS_PIN, OUTPUT);
  digitalWrite(RADIO_NSS_PIN, HIGH);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  int state = gRadio.begin(RADIO_FREQUENCY, RADIO_BANDWIDTH,
                           RADIO_SPREADING_FACTOR, RADIO_CODING_RATE,
                           RADIO_SYNC_WORD, RADIO_TX_POWER,
                           RADIO_PREAMBLE_LENGTH, RADIO_TCXO_VOLTAGE, false);
  if (state != RADIOLIB_ERR_NONE) {
    char buf[48];
    snprintf(buf, sizeof(buf), "RadioLib init %d", state);
    _setError(buf);
    return false;
  }
  gRadio.setDio2AsRfSwitch(true);
  gRadio.setCurrentLimit(140);
  gRadio.setPacketReceivedAction(onRadioPacketReceived);
  state = gRadio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    char buf[48];
    snprintf(buf, sizeof(buf), "RadioLib rx %d", state);
    _setError(buf);
    return false;
  }
  return true;
#else
  pinMode(RADIO_NSS_PIN, OUTPUT);
  digitalWrite(RADIO_NSS_PIN, HIGH);
  _setError("RadioLib disabled");
  return false;
#endif
}

bool RadioManager::_hardwareSend(const char* packet) {
#if RADIO_USE_RADIOLIB
  gRadio.setPacketSentAction(onRadioPacketSent);
  int state = gRadio.startTransmit(packet);
  if (state != RADIOLIB_ERR_NONE) {
    char buf[48];
    snprintf(buf, sizeof(buf), "RadioLib tx %d", state);
    _setError(buf);
    gRadio.startReceive();
    return false;
  }
  _setError("");
  return true;
#else
  (void)packet;
  _setError("RadioLib disabled");
  return false;
#endif
}

bool RadioManager::_hardwareReceive() {
#if RADIO_USE_RADIOLIB
  bool didWork = false;

  if (gRadioTxFlag) {
    gRadioTxFlag = false;
    _txInProgress = false;
    _setTxStatus("TX ok");
    gRadio.setPacketReceivedAction(onRadioPacketReceived);
    int state = gRadio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      char buf[48];
      snprintf(buf, sizeof(buf), "RadioLib rx %d", state);
      _setError(buf);
      _setState(RadioState::Error);
    } else {
      _setError("");
      _setState(RadioState::Ready);
    }
    didWork = true;
  }

  if (gRadioRxFlag) {
    gRadioRxFlag = false;
    size_t len = gRadio.getPacketLength();
    if (len > RADIO_MAX_PACKET_LEN) {
      uint8_t discard[RADIO_MAX_PACKET_LEN];
      gRadio.readData(discard, sizeof(discard));
      _setError("Packet too long");
      gRadio.startReceive();
      return true;
    }
    char buf[RADIO_MAX_PACKET_LEN + 1];
    memset(buf, 0, sizeof(buf));
    int state = gRadio.readData((uint8_t*)buf, len);
    if (state == RADIOLIB_ERR_NONE) {
      buf[len] = '\0';
      char err[48] = "";
      if (parsePacket(buf, _rxPacket, err, sizeof(err))) {
        _lastRSSI = (int)gRadio.getRSSI();
        _lastSNR = gRadio.getSNR();
        _rxPacket.rssi = _lastRSSI;
        _rxPacket.snr = _lastSNR;
        _hasRxPacket = true;
        _setState(RadioState::Rx);
        _setError("");
      } else {
        _setError(err);
      }
    } else {
      char err[48];
      snprintf(err, sizeof(err), "RadioLib read %d", state);
      _setError(err);
    }
    gRadio.startReceive();
    _setState(RadioState::Ready);
    didWork = true;
  }
  return didWork;
#else
  return false;
#endif
}

unsigned long RadioManager::_broadcastInterval() const {
  int pct = BatteryManager::instance().percentage();
  if (pct > 0 && pct <= 20) {
    return RADIO_BROADCAST_INTERVAL_LOW_POWER_MS;
  }
  GPSManager& gps = GPSManager::instance();
  if (gps.speedValid() && gps.speedKmph() < TRACK_STATIONARY_SPEED_KMPH) {
    return RADIO_BROADCAST_INTERVAL_LOW_POWER_MS;
  }
  return RADIO_BROADCAST_INTERVAL_MS;
}
