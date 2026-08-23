#include "storage.h"
#include <EEPROM.h>
#ifdef ESP32
#include <Preferences.h>
#endif
#include "ai.h"
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
  if (!prefs_ready) {
    prefs_ready = prefs.begin("eeprom", false);
    if (!prefs_ready) logger::error("Preferences begin FAILED - defaults will be used");
  }
}
uint8_t read(int addr) { return prefs.getUChar(String(addr).c_str(), 0); }
void write(int addr, uint8_t val) { prefs.putUChar(String(addr).c_str(), val); }
template<typename T> bool get(int addr, T &obj) {
  // IMPORTANT: prefs.getBytes() leaves the buffer untouched when the key is
  // missing or corrupted. Zero-fill first so unprovisioned slots never expose
  // stack garbage (this was the "random fade on every sensor" bug).
  memset(&obj, 0, sizeof(T));
  size_t n = prefs.getBytes(String(addr).c_str(), &obj, sizeof(T));
  return n == sizeof(T);
}
template<typename T> bool put(int addr, const T &obj) {
  return prefs.putBytes(String(addr).c_str(), &obj, sizeof(T)) == sizeof(T);
}
void commit() {}
void clearAll() { prefs.clear(); }

#else
void begin() { EEPROM.begin(EEPROM_SIZE); }
uint8_t read(int addr) { return EEPROM.read(addr); }
void write(int addr, uint8_t val) { EEPROM.write(addr, val); }
template<typename T> bool get(int addr, T &obj) { EEPROM.get(addr, obj); return true; }
template<typename T> bool put(int addr, const T &obj) { EEPROM.put(addr, obj); return true; }
void commit() { EEPROM.commit(); }
void clearAll() {
  memset(EEPROM.getDataPtr() + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_GENSET_START + 12, 0, EEPROM_SIZE - EEPROM_GENSET_START - 12);
}

#endif

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
  // Exception 28 (LOAD Prohibited) on every OTA enable -- the reported crash.
  // Use the chip-unique MAC token (GET_CHIP_ID in config.h) instead: no flash
  // deref (cannot fault) and stable across firmware updates, so with no
  // ArduinoOTA.onEnd() re-provisioning the gate cannot self-disable OTA after
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
    begin();
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
  begin();
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

// ================= AI CONFIG =================
// Persisted layout mirrors config.h EEPROM_AI_* sizes. magic/version gate the
// whole block: missing/corrupt -> all defaults (AI disabled, empty slots), so
// pre-AI installations boot unchanged and factory reset clears the block.

static const uint32_t AI_MAGIC = 0x514D4149;  // "QMAI"
static const uint16_t AI_VERSION = 1;

struct __attribute__((packed)) AiHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
};

struct __attribute__((packed)) AiGlobalPersist {
  uint8_t enabled;
  uint8_t provider;
  char endpoint[64];
  char api_key[64];
  char model[32];
  uint16_t timeout_ms;
  uint32_t rate_limit_ms;
};

struct __attribute__((packed)) AiPromptPersist {
  uint8_t enabled;
  uint8_t out_type;
  char name[17];
  char prompt[113];
  char model[32];
  float analog_min;
  float analog_max;
  uint32_t interval_ms;
};

static_assert(sizeof(AiGlobalPersist) == EEPROM_AI_GLOBAL_SIZE,
              "AiGlobalPersist size must match EEPROM_AI_GLOBAL_SIZE in config.h");
static_assert(sizeof(AiPromptPersist) == EEPROM_AI_SLOT_SIZE,
              "AiPromptPersist size must match EEPROM_AI_SLOT_SIZE in config.h");

void loadAi() {
  begin();
  int base = EEPROM_AI_START;
  AiHeader h = {};
  if (!get(base, h) || h.magic != AI_MAGIC || h.version != AI_VERSION ||
      h.count != AI_MAX_PROMPTS) {
    return;  // unprovisioned/corrupt: keep runtime defaults (AI disabled)
  }
  int addr = base + sizeof(AiHeader);

  AiGlobalPersist g = {};
  if (get(addr, g)) {
    ai::Config cfg = {};
    cfg.provider = (g.provider <= ai::PROVIDER_CUSTOM) ?
                   (ai::Provider)g.provider : ai::PROVIDER_OPENAI;
    strncpy(cfg.endpoint, g.endpoint, sizeof(cfg.endpoint) - 1);
    strncpy(cfg.api_key, g.api_key, sizeof(cfg.api_key) - 1);
    strncpy(cfg.model, g.model[0] ? g.model : "gpt-4o-mini", sizeof(cfg.model) - 1);
    cfg.timeout_ms = g.timeout_ms ? g.timeout_ms : 10000;
    cfg.rate_limit_ms = g.rate_limit_ms ? g.rate_limit_ms : 5000;
    cfg.enabled = (g.enabled == 1);
    ai::setConfig(cfg);
  }
  addr += sizeof(AiGlobalPersist);

  for (int i = 0; i < AI_MAX_PROMPTS; i++) {
    AiPromptPersist s = {};
    if (!get(addr, s)) { addr += sizeof(AiPromptPersist); continue; }
    ai::PromptCfg p = {};
    p.enabled = (s.enabled == 1);
    p.out_type = (ai::OutType)s.out_type;
    strncpy(p.name, s.name, sizeof(p.name) - 1);
    strncpy(p.model, s.model, sizeof(p.model) - 1);
    p.analog_min = s.analog_min;
    p.analog_max = s.analog_max;
    p.interval_ms = s.interval_ms;
    ai::setPrompt((uint8_t)i, p);  // re-validates + normalizes
    addr += sizeof(AiPromptPersist);
  }
}

// ---- prompt text accessors (text is NOT mirrored into runtime RAM) ----

static int aiSlotAddr(uint8_t idx) {
  return EEPROM_AI_START + sizeof(AiHeader) + sizeof(AiGlobalPersist) +
         idx * sizeof(AiPromptPersist);
}

bool getAiPromptText(uint8_t idx, char *out, size_t cap) {
  if (idx >= AI_MAX_PROMPTS || !out || cap == 0) return false;
  out[0] = '\0';
  begin();
  AiPromptPersist s = {};
  if (!get(aiSlotAddr(idx), s)) return false;
  strncpy(out, s.prompt, cap - 1);
  out[cap - 1] = '\0';
  return true;
}

bool saveAiPromptText(uint8_t idx, const char *text) {
  if (idx >= AI_MAX_PROMPTS || !text) return false;
  begin();
  int addr = aiSlotAddr(idx);
  AiPromptPersist s = {};
  get(addr, s);
  strncpy(s.prompt, text, sizeof(s.prompt) - 1);
  s.prompt[sizeof(s.prompt) - 1] = '\0';
  put(addr, s);
  commit();
  return true;
}

void saveAi() {
  begin();
  int base = EEPROM_AI_START;

  const ai::Config &cfg = ai::getConfig();
  AiGlobalPersist g = {};
  g.enabled = cfg.enabled ? 1 : 0;
  g.provider = (uint8_t)cfg.provider;
  strncpy(g.endpoint, cfg.endpoint, sizeof(g.endpoint) - 1);
  strncpy(g.api_key, cfg.api_key, sizeof(g.api_key) - 1);
  strncpy(g.model, cfg.model, sizeof(g.model) - 1);
  g.timeout_ms = cfg.timeout_ms;
  g.rate_limit_ms = cfg.rate_limit_ms;

  bool dirty = false;
  AiHeader h;
  h.magic = AI_MAGIC;
  h.version = AI_VERSION;
  h.count = AI_MAX_PROMPTS;

  int addr = base;
  AiHeader stored_h;
  get(addr, stored_h);
  if (memcmp(&h, &stored_h, sizeof(AiHeader)) != 0) {
    dirty = true;
    put(addr, h);
  }
  addr += sizeof(AiHeader);

  AiGlobalPersist stored_g;
  get(addr, stored_g);
  if (memcmp(&g, &stored_g, sizeof(AiGlobalPersist)) != 0) {
    dirty = true;
    put(addr, g);
  }
  addr += sizeof(AiGlobalPersist);

  for (int i = 0; i < AI_MAX_PROMPTS; i++) {
    const ai::PromptCfg &p = ai::getPrompt((uint8_t)i);
    AiPromptPersist s = {};
    s.enabled = p.enabled ? 1 : 0;
    s.out_type = (uint8_t)p.out_type;
    strncpy(s.name, p.name, sizeof(s.name) - 1);
    strncpy(s.model, p.model, sizeof(s.model) - 1);
    s.analog_min = p.analog_min;
    s.analog_max = p.analog_max;
    s.interval_ms = p.interval_ms;

    AiPromptPersist stored_s;
    get(addr, stored_s);
    // Prompt text is managed via saveAiPromptText(); metadata saves must
    // preserve whatever text is already persisted.
    memcpy(s.prompt, stored_s.prompt, sizeof(s.prompt));
    if (memcmp(&s, &stored_s, sizeof(AiPromptPersist)) != 0) {
      dirty = true;
      put(addr, s);
    }
    addr += sizeof(AiPromptPersist);
  }

  if (dirty) commit();
}

}  // namespace storage
