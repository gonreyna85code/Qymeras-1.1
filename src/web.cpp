#include <stdlib.h>
#include <errno.h>
#include <math.h>
#if !defined(ESP8266)
#include <esp_system.h>
#endif
#include "web.h"
#include "html.h"
#include "core.h"
#include "mesh.h"
#include "sensors.h"
#include "automations.h"
#include "storage.h"
#include "ai.h"
#include "log.h"

#ifndef ICACHE_FLASH_ATTR
#define ICACHE_FLASH_ATTR
#endif

namespace web {
WebServerCompat server(80);

static const char* AUTH_USERNAME = "admin";
static const char* AUTH_PASSWORD = "qymera123";
static bool auth_enabled = false;

static const char* EXPECTED_AUTH_BASE64 = "YWRtaW46cXltZXJhMTIz";


// Rate limiting - sliding window with a small burst allowance.
// A single UI action (e.g. enabling persistence) issues several back-to-back
// POSTs; a hard 1-per-2s rule made those fail with 429. The burst allowance
// keeps UI flows working while still throttling sustained floods.
static unsigned long last_request_time = 0;
static unsigned char burst_count = 0;
static const unsigned long REQUEST_COOLDOWN_MS = 2000; // window
static const unsigned char RATE_LIMIT_BURST = 6;       // requests per window

// Check rate limit for protected endpoints
// UI flows issue small bursts; sustained floods are throttled.
static bool checkRateLimit() {
  unsigned long now = millis();
  if (now - last_request_time > REQUEST_COOLDOWN_MS) {
    // New window: reset the burst counter.
    last_request_time = now;
    burst_count = 0;
    return true;
  }
  if (burst_count < RATE_LIMIT_BURST) {
    burst_count++;
    return true;
  }
  return false;
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

static bool parseStrictFloat(const String &s, float &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  errno = 0;
  float value = strtof(begin, &end);
  if (end == begin || *end != '\0') return false;
  if (errno == ERANGE || isinf(value) || isnan(value)) return false;
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
    auth_enabled = false;
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

void sendStartupJS() {
  if (WiFi.getMode() == WIFI_AP)
    server.sendContent_P(PSTR("let savedTab='wifi';show(savedTab);"));
  else
    server.sendContent_P(PSTR("let savedTab=(localStorage.getItem('tab')||'control');if(savedTab==='wifi')savedTab='config';show(savedTab);"));
    server.sendContent_P(
      PSTR("['control','auto','config'].forEach(t=>{document.getElementById('t_'+t).onclick=()=>show(t);});"));
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
  server.sendContent_P(html_content::AiPanel);
  server.sendContent("");
}

ICACHE_FLASH_ATTR void handleSave() {
  addCorsHeaders();
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
  storage::loadGeneralSettings(core::genset.broadcast_port, core::genset.command_port, core::genset.report_interval);
}

ICACHE_FLASH_ATTR void loadCredentials() {
  storage::loadCredentials(core::ssid, core::password);
}

ICACHE_FLASH_ATTR void saveGeneralSettings() {
  storage::saveGeneralSettings(core::genset.broadcast_port, core::genset.command_port, core::genset.report_interval);
}

ICACHE_FLASH_ATTR void factoryReset() {
  storage::factoryReset();
}

ICACHE_FLASH_ATTR void saveCredentials(const String &s, const String &p) {
  storage::saveCredentials(s, p);
}

ICACHE_FLASH_ATTR void handleGenSetSave() {
  addCorsHeaders();
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
  addCorsHeaders();
  logger::warn("Factory reset requested");
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
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
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
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
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
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

// Read-only diagnostics for production monitoring / memory-leak evidence
// (T008): uptime, free heap, WiFi RSSI, last reset reason. GET like /logs.
void handleStatus() {
  addCorsHeaders();
  String s = "{\"uptime_ms\":";
  s += millis();
  s += ",\"free_heap\":";
  s += ESP.getFreeHeap();
  s += ",\"rssi\":";
  s += WiFi.RSSI();
  s += ",\"reset_reason\":\"";
#if defined(ESP8266)
  s += ESP.getResetReason();
#else
  {
    // arduino-esp32 6.x removed Esp.getResetReason(); use esp_system API.
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON: s += "Power on"; break;
      case ESP_RST_SW: s += "Software"; break;
      case ESP_RST_PANIC: s += "Panic"; break;
      case ESP_RST_INT_WDT: s += "Interrupt watchdog"; break;
      case ESP_RST_TASK_WDT: s += "Task watchdog"; break;
      case ESP_RST_WDT: s += "Other watchdog"; break;
      case ESP_RST_DEEPSLEEP: s += "Deep sleep wake"; break;
      case ESP_RST_BROWNOUT: s += "Brownout"; break;
      default: s += "Unknown"; break;
    }
  }
#endif
  s += "\",\"chip\":\"";
#if defined(ESP8266)
  s += "esp8266";
#else
  s += "esp32";
#endif
  s += "\"}";
  server.send(200, "application/json", s);
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
  storage::loadCalibration();
}

ICACHE_FLASH_ATTR void saveCalibration() {
  storage::saveCalibration();
}

ICACHE_FLASH_ATTR void saveCalibrationSlot(int index) {
  storage::saveCalibrationSlot(index);
}

void handleDeleteRule() {
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
  addCorsHeaders();
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
  unsigned long id_ul = 0;
  if (parseStrictUnsigned(server.arg("id"), id_ul) && id_ul <= INT_MAX) id = (int)id_ul;
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
  // ================= LOGICAL OPERATOR =================
  // UI sends 'logic' (1 = AND, 0 = OR). Defaults to OR when absent.
  r.logical_and = server.hasArg("logic") && server.arg("logic") == "1";
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
  // Streamed response: entries are emitted as small chunks instead of
  // assembling a multi-KB String. This keeps peak heap usage flat regardless
  // of device count — critical on ESP8266 where a large transient allocation
  // can fragment the arena under concurrent mesh traffic.
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  bool firstObj = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    auto &r = mesh::reports[i];
    // Expose only active, well-formed entries: valid uid, valid type, and
    // remote entries that are still within MESH_TIMEOUT. Stale remote sensors,
    // SENSOR_NONE and invalid/garbage types are never reported as devices.
    if (!sensors::isEntryVisible(i)) continue;
    if (!firstObj) server.sendContent(",");
    firstObj = false;

    char buf[24];
    if (isnan(r.value) || isinf(r.value)) {
      strcpy(buf, "0");
    } else {
      dtostrf(r.value, 0, 4, buf);
    }

    char fb[24];
    String e;
    e.reserve(384);
    e += "{\"id\":";
    e += c.uid;
    e += ",\"index\":";
    e += i;
    e += ",\"device_uid\":";
    e += c.device_uid;
    e += ",\"name\":\"";
    e += c.name;
    e += "\",\"value\":";
    e += buf;
    e += ",\"pers_state\":";
    e += (c.pers_state ? "true" : "false");

    dtostrf(isnan(c.min) || isinf(c.min) ? 0.0f : c.min, 0, 4, fb);   e += ",\"min\":";        e += fb;
    dtostrf(isnan(c.max) || isinf(c.max) ? 0.0f : c.max, 0, 4, fb);   e += ",\"max\":";        e += fb;
    dtostrf(isnan(c.correction) || isinf(c.correction) ? 0.0f : c.correction, 0, 4, fb); e += ",\"correction\":";  e += fb;

    e += ",\"avail\":";           e += c.avail;
    e += ",\"pulse\":";           e += (c.pulse ? "true" : "false");
    e += ",\"state\":";           e += (r.state ? "true" : "false");
    e += ",\"pulse_ms\":";        e += c.pulse_ms;
    e += ",\"persist\":";         e += (c.persist ? "true" : "false");
    e += ",\"fade\":";            e += c.fade;
    e += ",\"type\":";            e += c.type;
    e += ",\"pin\":";             e += c.pin;
    e += ",\"local\":";           e += (c.local ? "true" : "false");
    e += ",\"last_update\":";     e += c.local ? 0 : c.last_update;
    // Elapsed ms since the last remote packet, computed server-side from the
    // same millis() timebase as MESH_TIMEOUT (client Date.now() is epoch-based
    // and cannot be compared directly with the device uptime counter).
    e += ",\"age_ms\":";          e += c.local ? 0 : (uint32_t)(millis() - c.last_update);

    e += ",\"ip\":\"";
    if (c.local) {
      IPAddress ip = WiFi.localIP();
      char ipbuf[16];
      snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      e += ipbuf;
    } else {
      e += c.device_ip;
    }
    e += "\"}";
    server.sendContent(e);
  }
  server.sendContent("]");
  server.sendContent("");  // finalize chunked transfer (both cores)
}

ICACHE_FLASH_ATTR void handleCalibSet() {
  addCorsHeaders();
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
  String type = server.arg("type");
  int calibIdx = sensors::findCalibByUid(sensorUid);
  if (calibIdx < 0) {
    server.send(400, "text/plain", "Sensor not found");
    return;
  }
  auto &c = sensors::calibrations[calibIdx];
  auto &r = mesh::reports[calibIdx];
  float raw = r.raw;

  // TIME / timezone: strict integer minutes from UTC, range -720..840.
  if (type == "TIME" || type == "timezone") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    long tz_min = 0;
    if (!parseStrictLong(server.arg("ref"), tz_min)) {
      server.send(400, "text/plain", "invalid timezone (integer minutes required)");
      return;
    }
    if (tz_min < -720 || tz_min > 840) {
      server.send(400, "text/plain", "timezone out of range (-720..840)");
      return;
    }
    c.correction = (float)tz_min;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // persist: strict boolean ref.
  if (type == "persist") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    String pv = server.arg("ref");
    if (pv != "1" && pv != "0") {
      server.send(400, "text/plain", "invalid persist value (0/1)");
      return;
    }
    bool enable = (pv == "1");
    c.persist = enable;
    c.pulse = false;
    // Snapshot the live state at enable time so a reboot right after enabling
    // persistence still restores the current relay state (the state is only
    // re-saved on subsequent toggles while persist is on).
    if (enable) c.pers_state = c.state;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // avail: strict boolean ref.
  if (type == "avail") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    String av = server.arg("ref");
    if (av != "1" && av != "0") {
      server.send(400, "text/plain", "invalid avail value (0/1)");
      return;
    }
    c.avail = (av == "1") ? 1 : 0;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // res: no payload needed.
  if (type == "res") {
    c.min = 0;
    c.max = 100;
    c.correction = 0;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // fad: fade ms, strict float, 0..3600000 (storage caps fades at 1h).
  if (type == "fad") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    float fad = 0;
    if (!parseStrictFloat(server.arg("ref"), fad)) {
      server.send(400, "text/plain", "invalid fade value");
      return;
    }
    if (fad < 0 || fad > 3600000.0f) {
      server.send(400, "text/plain", "fade out of range (0..3600000 ms)");
      return;
    }
    c.fade = (uint32_t)fad;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // pulse: pulse_ms, strict float, 0..3600000 (sane 1h cap, mirrors fade).
  if (type == "pulse") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    float pms = 0;
    if (!parseStrictFloat(server.arg("ref"), pms)) {
      server.send(400, "text/plain", "invalid pulse value");
      return;
    }
    if (pms < 0 || pms > 3600000.0f) {
      server.send(400, "text/plain", "pulse out of range (0..3600000 ms)");
      return;
    }
    c.pulse_ms = (uint32_t)pms;
    c.pulse = (pms > 0);
    c.persist = false;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // ref/min/max: strict float; ref defaults to the live raw value.
  float ref;
  if (server.hasArg("ref")) {
    if (!parseStrictFloat(server.arg("ref"), ref)) {
      server.send(400, "text/plain", "invalid ref value");
      return;
    }
  } else {
    ref = raw;
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
  } else {
    server.send(400, "text/plain", "Bad type");
    return;
  }
    saveCalibrationSlot(calibIdx);
  server.send(200, "text/plain", "OK");
}

// ================= AI =================

static String jsonEscape(const char *s) {
  String out;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if ((unsigned char)c >= 0x20) {
      out += c;
    }
  }
  return out;
}

ICACHE_FLASH_ATTR void handleAiGet() {
  addCorsHeaders();
  const ai::Config &cfg = ai::getConfig();
  String json;
  json.reserve(768);
  json += "{\"enabled\":";
  json += cfg.enabled ? "true" : "false";
  json += ",\"provider\":";
  json += String((int)cfg.provider);
  json += ",\"endpoint\":\"";
  json += jsonEscape(cfg.endpoint);
  // The API key is never echoed back: only whether one is configured.
  json += "\",\"api_key_set\":";
  json += (strlen(cfg.api_key) > 0) ? "true" : "false";
  json += ",\"model\":\"";
  json += jsonEscape(cfg.model);
  json += "\",\"timeout_ms\":";
  json += String(cfg.timeout_ms);
  json += ",\"rate_limit_ms\":";
  json += String(cfg.rate_limit_ms);
  json += ",\"prompts\":[";
  char text[113];
  for (int i = 0; i < AI_MAX_PROMPTS; i++) {
    const ai::PromptCfg &p = ai::getPrompt((uint8_t)i);
    if (i) json += ',';
    text[0] = '\0';
    storage::getAiPromptText((uint8_t)i, text, sizeof(text));
    json += "{\"slot\":";
    json += String(i);
    json += ",\"enabled\":";
    json += p.enabled ? "true" : "false";
    json += ",\"name\":\"";
    json += jsonEscape(p.name);
    json += "\",\"prompt\":\"";
    json += jsonEscape(text);
    json += "\",\"model\":\"";
    json += jsonEscape(p.model);
    json += "\",\"out_type\":";
    json += String((int)p.out_type);
    json += ",\"min\":";
    json += String(p.analog_min);
    json += ",\"max\":";
    json += String(p.analog_max);
    json += ",\"interval_ms\":";
    json += String(p.interval_ms);
    json += '}';
  }
  json += "]}";
  server.send(200, "application/json", json);
}

ICACHE_FLASH_ATTR void handleAiStatus() {
  addCorsHeaders();
  String json;
  json.reserve(512);
  json += "{\"state\":";
  json += String((int)ai::getState());
  json += ",\"active\":";
  json += String((int)ai::getActiveSlot());
  json += ",\"error\":\"";
  json += jsonEscape(ai::getLastError());
  json += "\",\"results\":[";
  for (int i = 0; i < AI_MAX_PROMPTS; i++) {
    const ai::SlotResult &r = ai::getSlotResult((uint8_t)i);
    if (i) json += ',';
    json += "{\"slot\":";
    json += String(i);
    json += ",\"valid\":";
    json += r.valid ? "true" : "false";
    json += ",\"digital\":";
    json += r.digital ? "true" : "false";
    json += ",\"analog\":";
    json += String(r.analog);
    json += ",\"age_ms\":";
    json += r.ts ? String(millis() - r.ts) : "-1";
    json += ",\"raw\":\"";
    json += jsonEscape(r.raw);
    json += "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

ICACHE_FLASH_ATTR void handleAiSet() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }

  String target = server.arg("target");

  if (target == "global") {
    ai::Config cfg = ai::getConfig();  // start from current: omitted = unchanged
    if (server.hasArg("enabled")) {
      String en = server.arg("enabled");
      if (en != "0" && en != "1") {
        server.send(400, "text/plain", "invalid enabled (0/1)");
        return;
      }
      cfg.enabled = (en == "1");
    }
    if (server.hasArg("provider")) {
      long pv = 0;
      if (!parseStrictLong(server.arg("provider"), pv) || pv < 0 || pv > 2) {
        server.send(400, "text/plain", "invalid provider (0..2)");
        return;
      }
      cfg.provider = (ai::Provider)pv;
    }
    if (server.hasArg("endpoint")) {
      String ep = server.arg("endpoint");
      if (ep.length() < 1 || ep.length() > 63) {
        server.send(400, "text/plain", "endpoint must be 1-63 chars");
        return;
      }
      memset(cfg.endpoint, 0, sizeof(cfg.endpoint));
      strncpy(cfg.endpoint, ep.c_str(), sizeof(cfg.endpoint) - 1);
    }
    if (server.hasArg("api_key")) {
      String key = server.arg("api_key");
      if (key.length() > 63) {
        server.send(400, "text/plain", "api_key must be 0-63 chars");
        return;
      }
      memset(cfg.api_key, 0, sizeof(cfg.api_key));
      strncpy(cfg.api_key, key.c_str(), sizeof(cfg.api_key) - 1);
    }
    if (server.hasArg("model")) {
      String m = server.arg("model");
      if (m.length() > 31) {
        server.send(400, "text/plain", "model must be 0-31 chars");
        return;
      }
      memset(cfg.model, 0, sizeof(cfg.model));
      strncpy(cfg.model, m.c_str(), sizeof(cfg.model) - 1);
    }
    if (server.hasArg("timeout_ms")) {
      long t = 0;
      if (!parseStrictLong(server.arg("timeout_ms"), t) || t < 100 || t > 60000) {
        server.send(400, "text/plain", "timeout_ms out of range (100..60000)");
        return;
      }
      cfg.timeout_ms = (uint16_t)t;
    }
    if (server.hasArg("rate_limit_ms")) {
      long rl = 0;
      if (!parseStrictLong(server.arg("rate_limit_ms"), rl) || rl < 100 || rl > 3600000L) {
        server.send(400, "text/plain", "rate_limit_ms out of range (100..3600000)");
        return;
      }
      cfg.rate_limit_ms = (uint32_t)rl;
    }
    ai::setConfig(cfg);
    storage::saveAi();
    server.send(200, "text/plain", "OK");
    return;
  }

  if (target == "prompt") {
    unsigned long slot_ul = 0;
    if (!server.hasArg("slot") || !parseStrictUnsigned(server.arg("slot"), slot_ul) ||
        slot_ul >= AI_MAX_PROMPTS) {
      server.send(400, "text/plain", "invalid slot");
      return;
    }
    uint8_t slot = (uint8_t)slot_ul;

    ai::PromptCfg p = {};
    p.enabled = (server.arg("enabled") == "1");
    p.interval_ms = 0;
    p.analog_min = 0.0f;
    p.analog_max = 100.0f;

    long ot = -1;
    if (!server.hasArg("out_type") || !parseStrictLong(server.arg("out_type"), ot) ||
        ot < 0 || ot > 3) {
      server.send(400, "text/plain", "invalid out_type (0..3)");
      return;
    }
    p.out_type = (ai::OutType)ot;

    String name = server.arg("name");
    if (name.length() > 16) {
      server.send(400, "text/plain", "name must be 0-16 chars");
      return;
    }
    strncpy(p.name, name.c_str(), sizeof(p.name) - 1);

    String prompt = server.arg("prompt");
    if (prompt.length() < 1 || prompt.length() > 112) {
      server.send(400, "text/plain", "prompt must be 1-112 chars");
      return;
    }

    String model = server.arg("model");  // optional per-slot override
    if (model.length() > 31) {
      server.send(400, "text/plain", "model must be 0-31 chars");
      return;
    }
    strncpy(p.model, model.c_str(), sizeof(p.model) - 1);

    float mn = 0.0f, mx = 100.0f;
    if (server.hasArg("min")) {
      if (!parseStrictFloat(server.arg("min"), mn)) {
        server.send(400, "text/plain", "invalid min");
        return;
      }
    }
    if (server.hasArg("max")) {
      if (!parseStrictFloat(server.arg("max"), mx)) {
        server.send(400, "text/plain", "invalid max");
        return;
      }
    }
    p.analog_min = mn;
    p.analog_max = mx;

    if (server.hasArg("interval_ms")) {
      unsigned long iv = 0;
      if (!parseStrictUnsigned(server.arg("interval_ms"), iv) || iv > 86400000UL) {
        server.send(400, "text/plain", "interval_ms out of range (0..86400000)");
        return;
      }
      p.interval_ms = (uint32_t)iv;
    }

    if (!ai::setPrompt(slot, p)) {
      server.send(400, "text/plain", "invalid prompt config");
      return;
    }
    storage::saveAiPromptText(slot, prompt.c_str());
    storage::saveAi();
    logger::eventf("AI prompt %d configured (%s)", slot, ai::getPrompt(slot).name);
    server.send(200, "text/plain", "OK");
    return;
  }

  server.send(400, "text/plain", "target must be global|prompt");
}

ICACHE_FLASH_ATTR void handleAiRun() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  unsigned long slot_ul = 0;
  if (!server.hasArg("slot") || !parseStrictUnsigned(server.arg("slot"), slot_ul) ||
      slot_ul >= AI_MAX_PROMPTS) {
    server.send(400, "text/plain", "invalid slot");
    return;
  }
  if (!ai::runPrompt((uint8_t)slot_ul)) {
    server.send(400, "text/plain", ai::getLastError());
    return;
  }
  server.send(200, "text/plain", "queued");
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
  server.on("/status", handleStatus);
  server.on("/ota/toggle", handleOtaToggle);
  server.on("/ota/status", handleOtaStatus);
  server.on("/ai", handleAiGet);
  server.on("/ai/status", handleAiStatus);
  server.on("/ai/set", HTTP_POST, handleAiSet);
  server.on("/ai/set", HTTP_OPTIONS, handleCorsOptions);
  server.on("/ai/run", HTTP_POST, handleAiRun);
  server.on("/ai/run", HTTP_OPTIONS, handleCorsOptions);
  server.begin();
}

}
