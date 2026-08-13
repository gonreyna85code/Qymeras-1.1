#include <stdlib.h>
#include "web.h"
#include "html.h"
#include "core.h"
#include "mesh.h"
#include "sensors.h"
#include "automations.h"
#include "log.h"

#ifndef ICACHE_FLASH_ATTR
#define ICACHE_FLASH_ATTR
#endif

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefs;
static bool prefs_ready = false;
static void eeprom_begin() {
  if (!prefs_ready) {
    prefs_ready = prefs.begin("eeprom", false);
  }
}
static uint8_t eeprom_read(int addr) {
  return prefs.getUChar(String(addr).c_str(), 0);
}
static void eeprom_write(int addr, uint8_t val) {
  prefs.putUChar(String(addr).c_str(), val);
}
template<typename T> static void eeprom_get(int addr, T &obj) {
  prefs.getBytes(String(addr).c_str(), &obj, sizeof(T));
}
template<typename T> static void eeprom_put(int addr, const T &obj) {
  prefs.putBytes(String(addr).c_str(), &obj, sizeof(T));
}
static void eeprom_commit() {
  // Preferences auto-commits
}
#else
#include <EEPROM.h>
static void eeprom_begin() {
  EEPROM.begin(EEPROM_SIZE);
}
static uint8_t eeprom_read(int addr) {
  return EEPROM.read(addr);
}
static void eeprom_write(int addr, uint8_t val) {
  EEPROM.write(addr, val);
}
template<typename T> static void eeprom_get(int addr, T &obj) {
  EEPROM.get(addr, obj);
}
template<typename T> static void eeprom_put(int addr, const T &obj) {
  EEPROM.put(addr, obj);
}
static void eeprom_commit() {
  EEPROM.commit();
}
#endif

namespace web {
WebServerCompat server(80);

static const char* AUTH_USERNAME = "admin";
static const char* AUTH_PASSWORD = "qymera123";
static bool auth_enabled = false;

static const char* EXPECTED_AUTH_BASE64 = "YWRtaW46cXltZXJhMTIz";


// Rate limiting - max requests per minute per endpoint
static unsigned long last_request_time = 0;
static const unsigned long REQUEST_COOLDOWN_MS = 2000; // 2 seconds between requests

// Check rate limit for protected endpoints
// Global cooldown: 2s between requests to state-changing endpoints
// UI endpoints are not rate-limited and operate freely
static bool checkRateLimit() {
  unsigned long now = millis();
  if (now - last_request_time < REQUEST_COOLDOWN_MS) {
    return false;
  }
  // Update global timer only when rate limiting is in effect
  // (prevents UI requests from affecting the cooldown for control endpoints)
  last_request_time = now;
  return true;
}

static bool parseStrictUnsigned(const String &s, unsigned long &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  unsigned long value = strtoul(begin, &end, 10);
  if (end == begin || *end != '\0') return false;
  out = value;
  return true;
}

static bool parseStrictLong(const String &s, long &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  long value = strtol(begin, &end, 10);
  if (end == begin || *end != '\0') return false;
  out = value;
  return true;
}

// Check HTTP Basic Authentication
// Auth state: enabled when AUTH_USERNAME/AUTH_PASSWORD are set (non-empty)
// Returns true if credentials valid, or if auth is disabled for backward compatibility
// When auth is enabled and credentials invalid, server sends 401 automatically
static bool checkAuth() {
  // Determine if auth is enabled: check if constants are set to non-empty values
  static bool initialized = false;
  if (!initialized) {
    auth_enabled = (strlen(AUTH_USERNAME) > 0 && strlen(AUTH_PASSWORD) > 0);
    initialized = true;
  }

  // If auth is not enabled, allow all (backward compatible default)
  if (!auth_enabled) return true;

  // Auth is enabled - require valid credentials
  if (!server.hasHeader("Authorization")) {
    return false; // Missing auth header when auth is enabled
  }
  String auth = server.header("Authorization");
  if (auth.startsWith("Basic ")) {
    String received = auth.substring(6);
    // Compare against pre-encoded Base64 credential
    if (received == EXPECTED_AUTH_BASE64) {
      return true;
    }
  }
  return false;
}

static void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleCorsOptions() {
  addCorsHeaders();
  server.send(204, "text/plain", "");
}

struct __attribute__((packed)) CalibrationPersist {
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

ICACHE_FLASH_ATTR CalibrationPersist makePersist(const sensors::Calibration &c) {
  CalibrationPersist p = {};
  p.pers_state = c.pers_state;
  p.min = c.min;
  p.max = c.max;
  p.correction = c.correction;
  p.avail = c.avail;
  p.persist = c.persist;
  p.pulse = c.pulse;
  p.pulse_ms = c.pulse_ms;
  p.fade = c.fade;
  return p;
}

void sendStartupJS() {
  if (WiFi.getMode() == WIFI_AP)
    server.sendContent_P(PSTR("let savedTab='wifi';show(savedTab);"));
  else
    server.sendContent_P(PSTR("let savedTab=(localStorage.getItem('tab')||'control');show(savedTab);"));
  server.sendContent_P(
    PSTR("['control','auto','config','wifi'].forEach(t=>{document.getElementById('t_'+t).onclick=()=>show(t);});"));
  server.sendContent_P(
    PSTR("window.genset={broadcast_port:"));
  server.sendContent(String(core::genset.broadcast_port));
  server.sendContent_P(PSTR(",command_port:"));
  server.sendContent(String(core::genset.command_port));
  server.sendContent_P(PSTR(",report_interval:"));
  server.sendContent(String(core::genset.report_interval));
  server.sendContent_P(PSTR("};"));
}

ICACHE_FLASH_ATTR void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(html_content::Styles);
  server.sendContent_P(html_content::Tabs);
  sendStartupJS();
  server.sendContent_P(html_content::Rules);
  server.sendContent_P(html_content::CardsSettings);
  server.sendContent_P(html_content::DeviceCards);
  server.sendContent_P(html_content::JS);
  server.sendContent_P(html_content::AutoWizJS);
  server.sendContent("");
}

ICACHE_FLASH_ATTR void handleSave() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  // Validate SSID length (1-32 chars)
  if (ssid.length() < 1 || ssid.length() > 32) {
    server.send(400, "text/plain", "SSID must be 1-32 characters");
    return;
  }
  // Validate password length (1-64 chars)
  if (pass.length() < 1 || pass.length() > 64) {
    server.send(400, "text/plain", "Password must be 1-64 characters");
    return;
  }
  saveCredentials(ssid, pass);
  logger::coref("WiFi config saved (SSID:%s)", ssid.c_str());
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
  server.close();
  RESET_MCU();
}

ICACHE_FLASH_ATTR void loadGeneralSettings() {
  eeprom_begin();
  int addr = EEPROM_GENSET_START;
  eeprom_get(addr, core::genset.broadcast_port);
  addr += sizeof(uint16_t);
  eeprom_get(addr, core::genset.command_port);
  addr += sizeof(uint16_t);
  eeprom_get(addr, core::genset.report_interval);
  addr += sizeof(uint32_t);
  if (core::genset.broadcast_port < 1024 || core::genset.broadcast_port > 65500)
    core::genset.broadcast_port = BROADCAST_PORT;
  if (core::genset.command_port < 1024 || core::genset.command_port > 65500)
    core::genset.command_port = COMMAND_PORT;
  if (core::genset.report_interval < 5000 || core::genset.report_interval > 600000)  // max 10 min
    core::genset.report_interval = BROADCAST_INTERVAL;
}

ICACHE_FLASH_ATTR void loadCredentials() {
  eeprom_begin();
  int slen = eeprom_read(EEPROM_CRED_START);
  if (slen < 0 || slen > 32) slen = 0;
  char ssid_buf[33] = {0};
  for (int i = 0; i < slen; i++) {
    char c = eeprom_read(EEPROM_CRED_START + 1 + i);
    if (c == 0xFF || c == 0) break;
    ssid_buf[i] = c;
  }
  core::ssid = ssid_buf;

  int plen_addr = EEPROM_CRED_START + 1 + slen;
  int plen = eeprom_read(plen_addr);
  if (plen < 0 || plen > 64) plen = 0;
  char pass_buf[65] = {0};
  for (int i = 0; i < plen; i++) {
    char c = eeprom_read(plen_addr + 1 + i);
    if (c == 0xFF || c == 0) break;
    pass_buf[i] = c;
  }
  core::password = pass_buf;
}

ICACHE_FLASH_ATTR void saveGeneralSettings() {
  eeprom_begin();
  int addr = EEPROM_GENSET_START;
  if (core::genset.broadcast_port < 1024 || core::genset.broadcast_port > 65500)
    core::genset.broadcast_port = BROADCAST_PORT;
  if (core::genset.command_port < 1024 || core::genset.command_port > 65500)
    core::genset.command_port = COMMAND_PORT;
  if (core::genset.report_interval < 5000 || core::genset.report_interval > 600000)
    core::genset.report_interval = BROADCAST_INTERVAL;
  eeprom_put(addr, core::genset.broadcast_port);
  addr += sizeof(uint16_t);
  eeprom_put(addr, core::genset.command_port);
  addr += sizeof(uint16_t);
  eeprom_put(addr, core::genset.report_interval);
  addr += sizeof(uint32_t);
  eeprom_commit();
}

ICACHE_FLASH_ATTR void factoryReset() {
  eeprom_begin();

#if defined(ESP32)
  // Clear all preferences
  prefs.clear();
#else
  // --- Relay state ---
  memset(EEPROM.getDataPtr() + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);

  // --- WiFi credentials ---
  memset(EEPROM.getDataPtr() + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);

  // --- Calibration + rules: clear rest of EEPROM ---
  memset(EEPROM.getDataPtr() + EEPROM_GENSET_START + 12, 0, EEPROM_SIZE - EEPROM_GENSET_START - 12);

  // --- Genset config: write defaults ---
  int addr = EEPROM_GENSET_START;
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  eeprom_put(addr, def_broadcast);
  addr += sizeof(uint16_t);
  eeprom_put(addr, def_command);
  addr += sizeof(uint16_t);
  eeprom_put(addr, def_interval);
#endif

  eeprom_commit();
  delay(100);
  RESET_MCU();
}

ICACHE_FLASH_ATTR void saveCredentials(const String &s, const String &p) {
  eeprom_begin();
  eeprom_write(EEPROM_CRED_START, s.length());
  for (int i = 0; i < s.length(); i++) eeprom_write(EEPROM_CRED_START + 1 + i, s[i]);
  int offset = EEPROM_CRED_START + 1 + s.length();
  eeprom_write(offset, p.length());
  for (int i = 0; i < p.length(); i++) eeprom_write(offset + 1 + i, p[i]);
  eeprom_commit();
}

ICACHE_FLASH_ATTR void handleGenSetSave() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  if (server.hasArg("broadcast"))
    core::genset.broadcast_port = server.arg("broadcast").toInt();
  if (server.hasArg("command"))
    core::genset.command_port = server.arg("command").toInt();
  if (server.hasArg("interval"))
    core::genset.report_interval = server.arg("interval").toInt();
  saveGeneralSettings();
  logger::coref("Genset saved (bc:%u,cmd:%u,int:%u)",
    core::genset.broadcast_port,
    core::genset.command_port,
    core::genset.report_interval);
  server.sendHeader("Location", "/");
  server.send(200, "text/plain", "OK");
}

ICACHE_FLASH_ATTR void handleFactoryReset() {
  logger::warn("Factory reset requested");
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  server.send(200, "text/plain", "RESET");
  factoryReset();
}

void handleToggleApi() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t id = (uint32_t)id_ul;
  int idx = sensors::findCalibByUid(id);
  if (idx < 0) {
    server.send(404, "text/plain", "id not found");
    return;
  }
  auto &c = sensors::calibrations[idx];
  if (c.type != sensors::TYPE_RELAY && c.type != sensors::TYPE_DIMMER) {
    server.send(400, "text/plain", "invalid actuator type");
    return;
  }
  sensors::handleToggle(id);
  server.send(200, "text/plain", "OK");
}

void handleDimmerApi() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("value") || !server.hasArg("id")) {
    server.send(400, "text/plain", "id and value required");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t id = (uint32_t)id_ul;
  int idx = sensors::findCalibByUid(id);
  if (idx < 0) {
    server.send(404, "text/plain", "id not found");
    return;
  }
  if (sensors::calibrations[idx].type != sensors::TYPE_DIMMER) {
    server.send(400, "text/plain", "invalid actuator type");
    return;
  }
  int value = server.arg("value").toInt();
  sensors::handleDimmer(id, value);
  server.send(200, "text/plain", "OK");
}

void handleLogs() {
  addCorsHeaders();
  server.send(200, "application/json", logger::getRecentLogsJson());
}

void handleOtaToggle() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (server.hasArg("enabled")) {
    bool enable = server.arg("enabled") == "1";
    core::setOtaEnabled(enable);
    server.send(200, "text/plain", enable ? "OK" : "OFF");
    delay(500);
    RESET_MCU();
  } else {
    server.send(400, "text/plain", "enabled arg required");
  }
}

void handleOtaStatus() {
  addCorsHeaders();
  server.send(200, "application/json", core::isOtaEnabled() ? "{\"ota\":1}" : "{\"ota\":0}");
}

ICACHE_FLASH_ATTR void loadCalibration() {
  eeprom_begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p;
    eeprom_get(addr, p);
    auto &c = sensors::calibrations[i];
    Serial.printf(
      "LOAD %d pers=%d persist=%d\n",
      i,
      p.pers_state,
      p.persist);
    c.pers_state = p.pers_state;
    c.min = p.min;
    c.max = p.max;
    c.correction = p.correction;
    c.avail = p.avail;
    c.persist = p.persist;
    c.pulse = p.pulse;
    c.pulse_ms = p.pulse_ms;
    c.fade = p.fade;
    // No pisar c.value si el sensor ya fue registrado por initSatellite()
    // (uid != 0 indica que sensors::temperature()/humidity()/etc. lo registró)
    if (c.uid == 0) {
      c.value = 0;
    }
  }
}

ICACHE_FLASH_ATTR void saveCalibration() {
  eeprom_begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    auto &c = sensors::calibrations[i];
    CalibrationPersist current = {};
    if (c.local && c.uid != 0) {
      current = makePersist(c);
    }
    CalibrationPersist stored;
    eeprom_get(addr, stored);
    if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
      eeprom_put(addr, current);
    }
  }
  eeprom_commit();
}

ICACHE_FLASH_ATTR void saveCalibrationSlot(int index) {
  if (index < 0 || index >= MAX_PERSISTED_SENSORS) return;
  eeprom_begin();
  int addr = EEPROM_CALIB_START + index * sizeof(CalibrationPersist);
  auto &c = sensors::calibrations[index];
  CalibrationPersist current = {};
  if (c.local && c.uid != 0) {
    current = makePersist(c);
  }
  CalibrationPersist stored;
  eeprom_get(addr, stored);
  if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
    eeprom_put(addr, current);
    logger::sensorsf("Calib slot %d saved (uid:%u)", index, c.uid);
    eeprom_commit();
  }
}

void handleDeleteRule() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul >= MAX_RULES) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint8_t id = (uint8_t)id_ul;
  automations::deleteRule(id);
  logger::eventf("Rule %d deleted", id);
  server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleRules() {
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "GET required");
    return;
  }
  String json;
  json.reserve(4096);
  json += '[';
  bool first = true;
  for (int i = 0; i < MAX_RULES; i++) {
    const automations::Rule &r = automations::rules[i];
    if (r.sensor_count == 0 && r.actuator_count == 0)
      continue;
    if (!first) json += ',';
    first = false;
    json += "{\"id\":";
    json += i;
    json += ",\"sensors\":[";
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) json += ',';
      json += r.sensor_idxs[s];
    }
    json += "],\"type\":";
    json += r.type;
    json += ",\"logical_and\":";
    json += r.logical_and;
    json += ",\"cmp\":[";
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) json += ',';
      json += r.cmp[s];
    }
    json += "],\"threshold\":[";
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) json += ',';
      json += r.threshold[s];
    }
    json += "],\"actuators\":[";
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) json += ',';
      json += r.actuator_idxs[a];
    }
    json += "],\"actions\":[";
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) json += ',';
      json += r.actions[a];
    }
    json += "],\"levels\":[";
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) json += ',';
      json += r.levels[a];
    }
    json += "],\"delay_ms\":";
    json += r.delay_ms;
    json += ",\"cooldown_ms\":";
    json += r.cooldown_ms;
    json += ",\"time_s\":";
    json += r.time_s;
    json += ",\"interval_ms\":";
    json += r.interval_ms;
    json += ",\"year_start\":";
    json += r.year_start;
    json += ",\"year_end\":";
    json += r.year_end;
    json += ",\"month_start\":";
    json += r.month_start;
    json += ",\"month_end\":";
    json += r.month_end;
    json += ",\"day_start\":";
    json += r.day_start;
    json += ",\"day_end\":";
    json += r.day_end;
    json += '}';
  }
  json += ']';
  server.send(200, "application/json", json);
}

void handleSetRule() {
  using namespace automations;
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  int id = -1;
  long id_long = 0;
  if (parseStrictUnsigned(server.arg("id"), (unsigned long&)id_long)) {
    if (id_long <= INT_MAX) id = (int)id_long;
  }
  if (id < 0) {
    for (int i = 0; i < MAX_RULES; i++) {
      if (rules[i].sensor_count == 0 && rules[i].actuator_count == 0) {
        id = i;
        break;
      }
    }
  }
  if (id < 0 || id >= MAX_RULES) {
    server.send(400, "text/plain", "invalid id");
    return;
  }
  Rule &r = rules[id];
  memset(&r, 0, sizeof(Rule));
  // ================= TYPE =================
  if (!server.hasArg("type")) {
    server.send(400, "text/plain", "type required");
    return;
  }
  int ruleType = server.arg("type").toInt();
  if (ruleType < 0 || ruleType > 3) {
    server.send(400, "text/plain", "invalid type");
    return;
  }
  r.type = (RuleType)ruleType;
  // ================= SENSORS =================
  if (server.hasArg("sensors")) {
    String sensors_str = server.arg("sensors");
    String cmp_str = server.arg("cmp");
    String threshold_str = server.arg("threshold");
    int idx = 0;
    while (sensors_str.length() && idx < 5) {
      int comma = sensors_str.indexOf(',');
      String token = (comma == -1) ? sensors_str : sensors_str.substring(0, comma);
      unsigned long sensor_ul = 0;
      if (!parseStrictUnsigned(token, sensor_ul) || sensor_ul >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid sensor index");
        return;
      }
      int sensor_id = (int)sensor_ul;
      if (sensors::calibrations[sensor_id].uid == 0) {
        server.send(400, "text/plain", "sensor not configured");
        return;
      }
      r.sensor_idxs[idx] = sensor_id;
      // CMP
      int cmp_val = 0;
      if (cmp_str.length()) {
        int c = cmp_str.indexOf(',');
        String t = (c == -1) ? cmp_str : cmp_str.substring(0, c);
        long cmp_long = 0;
        if (!parseStrictUnsigned(t, (unsigned long&)cmp_long)) {
          server.send(400, "text/plain", "invalid comparator");
          return;
        }
        cmp_val = (int)cmp_long;
        if (cmp_val < 0 || cmp_val > 2) {
          server.send(400, "text/plain", "invalid comparator");
          return;
        }
        if (c != -1) cmp_str = cmp_str.substring(c + 1);
        else cmp_str = "";
      }
      r.cmp[idx] = (Comparator)cmp_val;
      // THRESHOLD
      int th = 0;
      if (threshold_str.length()) {
        int c = threshold_str.indexOf(',');
        String t = (c == -1) ? threshold_str : threshold_str.substring(0, c);
        long th_long = 0;
        if (!parseStrictLong(t, th_long)) {
          server.send(400, "text/plain", "threshold out of range");
          return;
        }
        th = (int)th_long;
        if (th < -1000 || th > 10000) {
          server.send(400, "text/plain", "threshold out of range");
          return;
        }
        if (c != -1) threshold_str = threshold_str.substring(c + 1);
        else threshold_str = "";
      }
      r.threshold[idx] = th;
      idx++;
      if (comma == -1) break;
      sensors_str = sensors_str.substring(comma + 1);
    }
    r.sensor_count = idx;
  }
  // ================= ACTUATORS =================
  if (server.hasArg("actuators")) {
    String actuators_str = server.arg("actuators");
    String actions_str = server.arg("actions");
    String levels_str = server.arg("levels");
    int idx = 0;
    while (actuators_str.length() && idx < 5) {
      int comma = actuators_str.indexOf(',');
      String token = (comma == -1) ? actuators_str : actuators_str.substring(0, comma);
      unsigned long actuator_ul = 0;
      if (!parseStrictUnsigned(token, actuator_ul) || actuator_ul >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid actuator index");
        return;
      }
      int actuator_id = (int)actuator_ul;
      auto &cal = sensors::calibrations[actuator_id];
      if (cal.uid == 0) {
        server.send(400, "text/plain", "actuator not configured");
        return;
      }
      if (cal.type != sensors::TYPE_RELAY && cal.type != sensors::TYPE_DIMMER) {
        server.send(400, "text/plain", "invalid actuator type");
        return;
      }
      r.actuator_idxs[idx] = actuator_id;
      int action = 2;
      if (actions_str.length()) {
        int c = actions_str.indexOf(',');
        String t = (c == -1) ? actions_str : actions_str.substring(0, c);
        long action_long = 0;
        if (!parseStrictUnsigned(t, (unsigned long&)action_long)) {
          server.send(400, "text/plain", "invalid action");
          return;
        }
        action = (int)action_long;
        if (action < 0 || action > 3) {
          server.send(400, "text/plain", "invalid action");
          return;
        }
        if (action == ACT_LEVEL && cal.type != sensors::TYPE_DIMMER) {
          server.send(400, "text/plain", "LEVEL only for dimmers");
          return;
        }
        if (c != -1) actions_str = actions_str.substring(c + 1);
        else actions_str = "";
      }
      r.actions[idx] = (ActionType)action;
      int level = 0;
      if (levels_str.length()) {
        int c = levels_str.indexOf(',');
        String t = (c == -1) ? levels_str : levels_str.substring(0, c);
        long level_long = 0;
        if (!parseStrictLong(t, level_long)) {
          server.send(400, "text/plain", "level out of range");
          return;
        }
        level = (int)level_long;
        if (level < 0 || level > 100) {
          server.send(400, "text/plain", "level out of range");
          return;
        }
        if (c != -1) levels_str = levels_str.substring(c + 1);
        else levels_str = "";
      }
      r.levels[idx] = level;
      idx++;
      if (comma == -1) break;
      actuators_str = actuators_str.substring(comma + 1);
    }
    r.actuator_count = idx;
  }
  // ================= VALIDACIONES GENERALES =================
  if (r.actuator_count == 0) {
    server.send(400, "text/plain", "at least one actuator required");
    return;
  }
  if ((r.type == RULE_EDGE || r.type == RULE_THRESHOLD) && r.sensor_count == 0) {
    server.send(400, "text/plain", "sensors required");
    return;
  }
  // ================= TIME =================
  if (r.type == RULE_TIME) {
    long time_s_long = 0;
    if (!parseStrictLong(server.arg("time_s"), time_s_long)) {
      server.send(400, "text/plain", "invalid time_s");
      return;
    }
    int time_s = (int)time_s_long;
    if (time_s < 0 || time_s > 86400) {
      server.send(400, "text/plain", "invalid time_s");
      return;
    }
    r.time_s = time_s;
    long ys_l = 0, ms_l = 0, ds_l = 0, ye_l = 0, me_l = 0, de_l = 0;
    if (!parseStrictLong(server.arg("year_start"), ys_l) ||
        !parseStrictLong(server.arg("month_start"), ms_l) ||
        !parseStrictLong(server.arg("day_start"), ds_l) ||
        !parseStrictLong(server.arg("year_end"), ye_l) ||
        !parseStrictLong(server.arg("month_end"), me_l) ||
        !parseStrictLong(server.arg("day_end"), de_l)) {
      server.send(400, "text/plain", "invalid date");
      return;
    }
    int ys = (int)ys_l, ms = (int)ms_l, ds = (int)ds_l, ye = (int)ye_l, me = (int)me_l, de = (int)de_l;
    bool hasDate = ys || ms || ds || ye || me || de;
    if (hasDate) {
      if (ys && (ys < 1970 || ys > 2100)) {
        server.send(400, "text/plain", "invalid year_start");
        return;
      }
      if (ye && (ye < 1970 || ye > 2100)) {
        server.send(400, "text/plain", "invalid year_end");
        return;
      }
      if (ms && (ms < 1 || ms > 12)) {
        server.send(400, "text/plain", "invalid month_start");
        return;
      }
      if (me && (me < 1 || me > 12)) {
        server.send(400, "text/plain", "invalid month_end");
        return;
      }
      if (ds && (ds < 1 || ds > 31)) {
        server.send(400, "text/plain", "invalid day_start");
        return;
      }
      if (de && (de < 1 || de > 31)) {
        server.send(400, "text/plain", "invalid day_end");
        return;
      }
      r.year_start = ys;
      r.month_start = ms;
      r.day_start = ds;
      r.year_end = ye;
      r.month_end = me;
      r.day_end = de;
      if (ys && ye && ys > ye) {
        server.send(400, "text/plain", "start > end");
        return;
      }
    }
  }

  // ================= INTERVAL =================
  if (r.type == RULE_INTERVAL) {
    long interval_long = 0;
    if (!parseStrictLong(server.arg("interval"), interval_long)) {
      server.send(400, "text/plain", "invalid interval");
      return;
    }
    int interval = (int)interval_long;
    if (interval < 1000 || interval > 3600000) {
      server.send(400, "text/plain", "invalid interval");
      return;
    }
    r.interval_ms = interval;
  }
  // ================= DELAY / COOLDOWN =================
  long delay_long = 0, cooldown_long = 0;
  if (!parseStrictLong(server.arg("delay"), delay_long) || !parseStrictLong(server.arg("cooldown"), cooldown_long)) {
    server.send(400, "text/plain", "invalid delay/cooldown");
    return;
  }
  r.delay_ms = delay_long;
  r.cooldown_ms = cooldown_long;
  saveRulesToEEPROM();
  logger::eventf("Rule %d saved (type:%d, sensors:%d, actuators:%d)",
    id, r.type, r.sensor_count, r.actuator_count);
  server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleCalib() {
  addCorsHeaders();
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String json;
  json.reserve(8192);
  json += '[';
  bool firstObj = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    auto &r = mesh::reports[i];
    if (c.uid == 0) continue;
    if (!firstObj) json += ',';
    firstObj = false;

    char buf[24];
    if (isnan(r.value) || isinf(r.value)) {
      strcpy(buf, "0");
    } else {
      dtostrf(r.value, 0, 4, buf);
    }

    json += "{\"id\":";
    json += c.uid;
    json += ",\"index\":";
    json += i;
    json += ",\"device_uid\":";
    json += c.device_uid;
    json += ",\"name\":\"";
    json += c.name;
    json += "\",\"value\":";
    json += buf;
    json += ",\"pers_state\":";
    json += (c.pers_state ? "true" : "false");

    char fb[24];
    dtostrf(c.min, 0, 4, fb);        json += ",\"min\":";        json += fb;
    dtostrf(c.max, 0, 4, fb);        json += ",\"max\":";        json += fb;
    dtostrf(c.correction, 0, 4, fb); json += ",\"correction\":";  json += fb;

    json += ",\"avail\":";           json += c.avail;
    json += ",\"pulse\":";           json += (c.pulse ? "true" : "false");
    json += ",\"state\":";           json += (r.state ? "true" : "false");
    json += ",\"pulse_ms\":";        json += c.pulse_ms;
    json += ",\"persist\":";         json += (c.persist ? "true" : "false");
    json += ",\"fade\":";            json += c.fade;
    json += ",\"type\":";            json += c.type;
    json += ",\"pin\":";             json += c.pin;
    json += ",\"local\":";           json += (c.local ? "true" : "false");

    json += ",\"ip\":\"";
    if (c.local) {
      IPAddress ip = WiFi.localIP();
      char ipbuf[16];
      snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      json += ipbuf;
    } else {
      json += c.device_ip;
    }
    json += "\"}";
  }
  json += ']';
  server.send(200, "application/json", json);
}

ICACHE_FLASH_ATTR void handleCalibSet() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  addCorsHeaders();
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  unsigned long sensorUidUl = 0;
  if (!parseStrictUnsigned(server.arg("id"), sensorUidUl) || sensorUidUl > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t sensorUid = (uint32_t)sensorUidUl;
  auto *cal = sensors::getCalib(server.arg("id"));
  (void)cal;
  String type = server.arg("type");
  int calibIdx = sensors::findCalibByUid(sensorUid);
  if (calibIdx < 0) {
    server.send(400, "text/plain", "Sensor not found");
    return;
  }
  auto &c = sensors::calibrations[calibIdx];
  auto &r = mesh::reports[calibIdx];
  float raw = r.raw;
  float ref = server.hasArg("ref") ? server.arg("ref").toFloat() : raw;
  if (type == "TIME") {
    c.correction = server.arg("ref").toInt();
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }
  if (type == "ref") {
    if (ref == 0) c.correction = 0;
    else {
      if (c.type == sensors::SENSOR_LUMI)
        ref = ref * 7074.0f / 108.9432f;
      c.correction = ref - raw;
    }
  } else if (type == "min") {
    c.min = raw + c.correction;
  } else if (type == "max") {
    c.max = raw + c.correction;
  } else if (type == "fad") {
    c.fade = ref;
  } else if (type == "pulse") {
    c.pulse_ms = ref;
    c.pulse = (ref > 0);
    c.persist = false;
  } else if (type == "persist") {
    bool enable = server.arg("ref") == "1";
    c.persist = enable;
    c.pulse = false;
  } else if (type == "avail") {
    c.avail = ref ? 1 : 0;
  } else if (type == "res") {
    c.min = 0;
    c.max = 100;
    c.correction = 0;
  } else if (type == "timezone") {
    c.correction = ref;
  } else {
    server.send(400, "text/plain", "Bad type");
    return;
  }
  saveCalibrationSlot(calibIdx);
  server.send(200, "text/plain", "OK");
}

void init() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/calib", handleCalib);
  server.on("/calib", HTTP_OPTIONS, handleCorsOptions);
  server.on("/calib/set", HTTP_POST, handleCalibSet);
  server.on("/calib/set", HTTP_OPTIONS, handleCorsOptions);
  server.on("/genset/save", HTTP_POST, handleGenSetSave);
  server.on("/rules", handleRules);
  server.on("/rules/set", HTTP_POST, handleSetRule);
  server.on("/rules/delete", HTTP_POST, handleDeleteRule);
  server.on("/factory", HTTP_POST, handleFactoryReset);
  server.on("/toggle", HTTP_POST, handleToggleApi);
  server.on("/toggle", HTTP_OPTIONS, handleCorsOptions);
  server.on("/dimmer", HTTP_POST, handleDimmerApi);
  server.on("/dimmer", HTTP_OPTIONS, handleCorsOptions);
  server.on("/logs", handleLogs);
  server.on("/ota/toggle", handleOtaToggle);
  server.on("/ota/status", handleOtaStatus);
  server.begin();
}

}
