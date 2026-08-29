#include "storage.h"
#include <nvs_flash.h>
#include "automations.h"
#include "core.h"
#include "log.h"
#include "mesh.h"
#include "sensors.h"

namespace storage {

/* =========================
   PERSISTENCE BACKEND (native NVS)
   The historical byte-addressed EEPROM (4 KiB) is kept as layout truth; the
   whole 4 KiB image lives in RAM (shadow) and is flushed to NVS as one blob on
   commit(). Unwritten bytes read 0xFF, matching classic EEPROM semantics.
   ========================= */

static nvs_handle_t nvs_h = 0;
static bool nvs_ready = false;
static uint8_t eeprom_img[EEPROM_SIZE];

void begin() {
  if (nvs_ready) return;
  nvs_ready = true;
  if (nvs_open("qymera", NVS_READWRITE, &nvs_h) == ESP_OK) {
    size_t sz = EEPROM_SIZE;
    esp_err_t err = nvs_get_blob(nvs_h, "eedata", eeprom_img, &sz);
    if (err == ESP_OK && sz == EEPROM_SIZE) return;
  }
  memset(eeprom_img, 0xFF, sizeof(eeprom_img));
}

uint8_t read(int addr) {
  begin();
  if (addr < 0 || addr >= EEPROM_SIZE) return 0xFF;
  return eeprom_img[addr];
}

void write(int addr, uint8_t val) {
  begin();
  if (addr < 0 || addr >= EEPROM_SIZE) return;
  eeprom_img[addr] = val;
}

template <typename T> bool get(int addr, T &obj) {
  begin();
  if (addr < 0 || (size_t)addr + sizeof(T) > EEPROM_SIZE) {
    memset(&obj, 0, sizeof(T));
    return false;
  }
  memcpy(&obj, eeprom_img + addr, sizeof(T));
  return true;
}

template <typename T> bool put(int addr, const T &obj) {
  begin();
  if (addr < 0 || (size_t)addr + sizeof(T) > EEPROM_SIZE) return false;
  memcpy(eeprom_img + addr, &obj, sizeof(T));
  return true;
}

void commit() {
  begin();
  if (!nvs_h) return;
  if (nvs_set_blob(nvs_h, "eedata", eeprom_img, EEPROM_SIZE) == ESP_OK) {
    nvs_commit(nvs_h);
  }
}

void clearAll() {
  begin();
  memset(eeprom_img + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);
  memset(eeprom_img + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);
  memset(eeprom_img + EEPROM_GENSET_START + 12, 0,
         EEPROM_SIZE - EEPROM_GENSET_START - 12);
}

/* Persisted calibration slot.
   magic/version validate the slot is provisioned and current; uid ties the
   slot to the registered device so persistence survives index reordering. */
static const uint32_t CALIB_MAGIC = 0x514D434C;  // "QMCL"
static const uint16_t CALIB_VERSION = 1;

struct __attribute__((packed)) CalibrationPersist {
  uint32_t magic;
  uint16_t version;
  uint32_t uid;
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
static_assert(sizeof(CalibrationPersist) == EEPROM_CALIB_SLOT_SIZE,
              "CalibrationPersist size must match EEPROM_CALIB_SLOT_SIZE in config.h");

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
  // Not a full-image hash: a prior `(uint8_t*)0x0000` deref here caused
  // Exception 28 (LOAD Prohibited) on every OTA enable. Use the chip-unique MAC
  // token (GET_CHIP_ID in config.h) instead: no flash deref (cannot fault) and
  // stable across firmware updates, so the gate cannot self-disable OTA after
  // an update. Best-effort / detection-only (no SHA-256), per design.
  return GET_CHIP_ID();
}

bool verifyOtaIntegrity() {
  begin();
  uint32_t expected = 0;
  get(EEPROM_OTA_HASH_ADDR, expected);
  commit();
  bool provisioned = (expected != 0xFFFFFFFFu && expected != 0u);
  uint32_t actual = calculateFirmwareHash();
  if (!provisioned) {
    put(EEPROM_OTA_HASH_ADDR, actual);
    commit();
    ota_integrity_verified = true;
    logger::core("OTA baseline hash stored (provisioning step)");
    return true;
  }
  ota_integrity_verified = (actual == expected);
  if (!ota_integrity_verified) logger::warnf("OTA integrity FAILED (stored:%08X actual:%08X)", expected, actual);
  else logger::core("OTA integrity verified");
  return ota_integrity_verified;
}

bool isOtaIntegrityVerified() { return ota_integrity_verified; }

uint8_t loadOtaFlag() {
  begin();
  // Normalize: only 1 means enabled. 0xFF (unprovisioned) or anything else = disabled.
  return (read(EEPROM_OTA_FLAG_ADDR) == 1) ? 1 : 0;
}

void saveOtaFlag(uint8_t flag) {
  write(EEPROM_OTA_FLAG_ADDR, (flag == 1) ? 1 : 0);
  commit();
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
  write(EEPROM_CRED_START, (uint8_t)ssid.length());
  for (int i = 0; i < (int)ssid.length(); i++) write(EEPROM_CRED_START + 1 + i, (uint8_t)ssid[i]);
  int offset = EEPROM_CRED_START + 1 + (int)ssid.length();
  write(offset, (uint8_t)password.length());
  for (int i = 0; i < (int)password.length(); i++) write(offset + 1 + i, (uint8_t)password[i]);
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
  clearAll();
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  put(addr, def_broadcast); addr += sizeof(uint16_t);
  put(addr, def_command); addr += sizeof(uint16_t);
  put(addr, def_interval);
  commit();
  delay(100);
  RESET_MCU();
}

void loadCalibration() {
  begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p = {};
    if (!get(addr, p)) continue;                    // missing/corrupt key on ESP32
    if (p.magic != CALIB_MAGIC || p.version != CALIB_VERSION) continue;  // unprovisioned
    if (p.uid == 0) continue;
    if (!isfinite(p.min) || !isfinite(p.max) || !isfinite(p.correction)) continue;
    if (p.fade > 3600000UL) continue;               // sane fade cap (1h)
    int idx = sensors::findCalibByUid(p.uid);       // attach to the exact device
    if (idx < 0) continue;                          // no registered device with this uid
    auto &c = sensors::calibrations[idx];
    if (!c.local) continue;
    c.pers_state = p.pers_state;
    c.min = p.min;
    c.max = p.max;
    c.correction = p.correction;
    c.avail = p.avail;
    c.persist = p.persist;
    c.pulse = p.pulse;
    c.pulse_ms = p.pulse_ms;
    c.fade = p.fade;
  }
}

void saveCalibrationSlot(int index) {
  if (index < 0 || index >= MAX_PERSISTED_SENSORS) return;
  begin();
  int addr = EEPROM_CALIB_START + index * sizeof(CalibrationPersist);
  auto &c = sensors::calibrations[index];
  CalibrationPersist current = {};
  if (c.local && c.uid != 0) {
    current.magic = CALIB_MAGIC;
    current.version = CALIB_VERSION;
    current.uid = c.uid;
    current.pers_state = c.pers_state;
    current.min = c.min;
    current.max = c.max;
    current.correction = c.correction;
    current.avail = c.avail;
    current.persist = c.persist;
    current.pulse = c.pulse;
    current.pulse_ms = c.pulse_ms;
    current.fade = c.fade;
  }
  CalibrationPersist stored = {};
  get(addr, stored);
  if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
    put(addr, current);
    commit();
  }
}

void saveCalibration() {
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