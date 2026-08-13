#include "storage.h"
#include <EEPROM.h>
#ifdef ESP32
#include <Preferences.h>
#endif
#include <ArduinoOTA.h>
#include "automations.h"
#include "core.h"
#include "log.h"
#include "mesh.h"
#include "sensors.h"

namespace storage {

#if defined(ESP32)
static Preferences prefs;
static bool prefs_ready = false;

void begin() {
  if (!prefs_ready) prefs_ready = prefs.begin("eeprom", false);
}
uint8_t read(int addr) { return prefs.getUChar(String(addr).c_str(), 0); }
void write(int addr, uint8_t val) { prefs.putUChar(String(addr).c_str(), val); }
template<typename T> void get(int addr, T &obj) { prefs.getBytes(String(addr).c_str(), &obj, sizeof(T)); }
template<typename T> void put(int addr, const T &obj) { prefs.putBytes(String(addr).c_str(), &obj, sizeof(T)); }
void commit() {}
void clearAll() { prefs.clear(); }

#else
void begin() { EEPROM.begin(EEPROM_SIZE); }
uint8_t read(int addr) { return EEPROM.read(addr); }
void write(int addr, uint8_t val) { EEPROM.write(addr, val); }
template<typename T> void get(int addr, T &obj) { EEPROM.get(addr, obj); }
template<typename T> void put(int addr, const T &obj) { EEPROM.put(addr, obj); }
void commit() { EEPROM.commit(); }
void clearAll() {
  memset(EEPROM.getDataPtr() + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_GENSET_START + 12, 0, EEPROM_SIZE - EEPROM_GENSET_START - 12);
}

#endif

struct CalibrationPersist {
  bool pers_state;
  float min;
  float max;
  float correction;
  uint8_t avail;
  bool persist;
  bool pulse;
  uint32_t pulse_ms;
  uint32_t fade;
};

struct RulesHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
};

static const uint32_t RULES_MAGIC = 0x4155544F;  // "AUTO"
static const uint16_t RULES_VERSION = 1;

static const uint32_t OTA_HASH_BYTES = 256;
static bool ota_integrity_verified = false;

static uint32_t calculateFirmwareHash() {
  uint32_t hash = 0;
  uint8_t *ptr = (uint8_t *)0x0000;
  uint32_t sketch_size = ESP.getSketchSize();
  int bytes_to_hash = (sketch_size < OTA_HASH_BYTES) ? sketch_size : OTA_HASH_BYTES;
  for (int i = 0; i < bytes_to_hash; i++) {
    hash = (hash << 5) | (hash >> 27);
    hash += ptr[i];
  }
  return hash;
}

bool verifyOtaIntegrity() {
  begin();
  uint32_t expected = read(EEPROM_OTA_CHECKSUM_ADDR);
  commit();
  if (expected == 0) {
    begin();
    write(EEPROM_OTA_CHECKSUM_ADDR, calculateFirmwareHash());
    commit();
    ota_integrity_verified = true;
    logger::core("OTA baseline hash stored (provisioning step)");
    return true;
  }
  uint32_t actual = calculateFirmwareHash();
  ota_integrity_verified = (actual == expected);
  if (!ota_integrity_verified) logger::warnf("OTA integrity FAILED (stored:%08X actual:%08X)", expected, actual);
  else logger::core("OTA integrity verified");
  return ota_integrity_verified;
}

bool isOtaIntegrityVerified() { return ota_integrity_verified; }

uint8_t loadOtaFlag() {
  begin();
  return read(EEPROM_OTA_FLAG_ADDR);
}

void saveOtaFlag(uint8_t flag) {
  begin();
  write(EEPROM_OTA_FLAG_ADDR, flag);
  commit();
}

void setOtaEnabled(bool enabled) {
  saveOtaFlag(enabled ? 1 : 0);
  if (enabled) {
    if (verifyOtaIntegrity()) {
      ArduinoOTA.begin();
      logger::core("OTA enabled (integrity OK)");
    } else {
      saveOtaFlag(0);
      logger::core("OTA integrity FAILED - keeping OTA disabled");
    }
  } else {
    logger::core("OTA disabled");
  }
}

bool isOtaEnabled() { return loadOtaFlag() == 1; }

void loadCredentials(String &ssid, String &password) {
  begin();
  int slen = read(EEPROM_CRED_START);
  if (slen < 0 || slen > 32) slen = 0;
  char ssid_buf[33] = {0};
  for (int i = 0; i < slen; i++) {
    char c = read(EEPROM_CRED_START + 1 + i);
    if (c == 0xFF || c == 0) break;
    ssid_buf[i] = c;
  }
  ssid = ssid_buf;
  int plen_addr = EEPROM_CRED_START + 1 + slen;
  int plen = read(plen_addr);
  if (plen < 0 || plen > 64) plen = 0;
  char pass_buf[65] = {0};
  for (int i = 0; i < plen; i++) {
    char c = read(plen_addr + 1 + i);
    if (c == 0xFF || c == 0) break;
    pass_buf[i] = c;
  }
  password = pass_buf;
}

void saveCredentials(const String &ssid, const String &password) {
  begin();
  write(EEPROM_CRED_START, ssid.length());
  for (int i = 0; i < ssid.length(); i++) write(EEPROM_CRED_START + 1 + i, ssid[i]);
  int offset = EEPROM_CRED_START + 1 + ssid.length();
  write(offset, password.length());
  for (int i = 0; i < password.length(); i++) write(offset + 1 + i, password[i]);
  commit();
}

void loadGeneralSettings(uint16_t &broadcast_port, uint16_t &command_port, uint32_t &report_interval) {
  begin();
  int addr = EEPROM_GENSET_START;
  get(addr, broadcast_port); addr += sizeof(uint16_t);
  get(addr, command_port); addr += sizeof(uint16_t);
  get(addr, report_interval); addr += sizeof(uint32_t);
  if (broadcast_port < 1024 || broadcast_port > 65500) broadcast_port = BROADCAST_PORT;
  if (command_port < 1024 || command_port > 65500) command_port = COMMAND_PORT;
  if (report_interval < 5000 || report_interval > 600000) report_interval = BROADCAST_INTERVAL;
}

void saveGeneralSettings(uint16_t broadcast_port, uint16_t command_port, uint32_t report_interval) {
  begin();
  if (broadcast_port < 1024 || broadcast_port > 65500) broadcast_port = BROADCAST_PORT;
  if (command_port < 1024 || command_port > 65500) command_port = COMMAND_PORT;
  if (report_interval < 5000 || report_interval > 600000) report_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  put(addr, broadcast_port); addr += sizeof(uint16_t);
  put(addr, command_port); addr += sizeof(uint16_t);
  put(addr, report_interval); addr += sizeof(uint32_t);
  commit();
}

void factoryReset() {
  begin();
#if defined(ESP32)
  prefs.clear();
#else
  clearAll();
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  put(addr, def_broadcast); addr += sizeof(uint16_t);
  put(addr, def_command); addr += sizeof(uint16_t);
  put(addr, def_interval);
#endif
  commit();
  delay(100);
  RESET_MCU();
}

void loadCalibration() {
  begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p;
    get(addr, p);
    auto &c = sensors::calibrations[i];
    c.pers_state = p.pers_state;
    c.min = p.min;
    c.max = p.max;
    c.correction = p.correction;
    c.avail = p.avail;
    c.persist = p.persist;
    c.pulse = p.pulse;
    c.pulse_ms = p.pulse_ms;
    c.fade = p.fade;
    if (c.uid == 0) c.value = 0;
  }
}

void saveCalibrationSlot(int index) {
  if (index < 0 || index >= MAX_PERSISTED_SENSORS) return;
  begin();
  int addr = EEPROM_CALIB_START + index * sizeof(CalibrationPersist);
  auto &c = sensors::calibrations[index];
  CalibrationPersist current = {};
  if (c.local && c.uid != 0) {
    current = {c.pers_state, c.min, c.max, c.correction, c.avail, c.persist, c.pulse, c.pulse_ms, c.fade};
  }
  CalibrationPersist stored;
  get(addr, stored);
  if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
    put(addr, current);
    commit();
  }
}

void saveCalibration() {
  begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) saveCalibrationSlot(i);
}

void loadRules() {
  begin();
  RulesHeader h;
  int addr = EEPROM_RULES_START;
  get(addr, h);
  addr += sizeof(RulesHeader);
  if (h.magic != RULES_MAGIC || h.version != RULES_VERSION) {
    memset(automations::rules, 0, sizeof(automations::rules));
    return;
  }
  for (int i = 0; i < MAX_RULES; i++) {
    get(addr, automations::rules[i]);
    addr += sizeof(automations::Rule);
  }
}

void saveRules() {
  begin();
  RulesHeader h;
  h.magic = RULES_MAGIC;
  h.version = RULES_VERSION;
  h.count = MAX_RULES;
  int addr = EEPROM_RULES_START;
  bool dirty = false;
  RulesHeader stored_h;
  get(addr, stored_h);
  if (memcmp(&h, &stored_h, sizeof(RulesHeader)) != 0) {
    dirty = true;
    put(addr, h);
  }
  addr += sizeof(RulesHeader);
  for (int i = 0; i < MAX_RULES; i++) {
    automations::Rule stored;
    get(addr, stored);
    if (memcmp(&automations::rules[i], &stored, sizeof(automations::Rule)) != 0) {
      dirty = true;
      put(addr, automations::rules[i]);
    }
    addr += sizeof(automations::Rule);
  }
  if (dirty) commit();
}

void deleteRule(uint8_t idx) {
  automations::deleteRule(idx);
}

}  // namespace storage
