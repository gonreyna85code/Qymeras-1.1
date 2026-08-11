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

namespace web {
WebServerCompat server(80);

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
  String ssid = server.arg("ssid");
  saveCredentials(ssid, server.arg("pass"));
  logger::coref("WiFi config saved (SSID:%s)", ssid.c_str());
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
  server.close();
  RESET_MCU();
}

ICACHE_FLASH_ATTR void loadGeneralSettings() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = EEPROM_GENSET_START;
  EEPROM.get(addr, core::genset.broadcast_port);
  addr += sizeof(uint16_t);
  EEPROM.get(addr, core::genset.command_port);
  addr += sizeof(uint16_t);
  EEPROM.get(addr, core::genset.report_interval);
  addr += sizeof(uint32_t);
  if (core::genset.broadcast_port < 1024 || core::genset.broadcast_port > 65500)
    core::genset.broadcast_port = BROADCAST_PORT;
  if (core::genset.command_port < 1024 || core::genset.command_port > 65500)
    core::genset.command_port = COMMAND_PORT;
  if (core::genset.report_interval < 5000 || core::genset.report_interval > 600000)  // max 10 min
    core::genset.report_interval = BROADCAST_INTERVAL;
}

ICACHE_FLASH_ATTR void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  int slen = EEPROM.read(EEPROM_CRED_START);
  if (slen < 0 || slen > 32) slen = 0;
  char ssid_buf[33] = {0};
  for (int i = 0; i < slen; i++) {
    char c = EEPROM.read(EEPROM_CRED_START + 1 + i);
    if (c == 0xFF || c == 0) break;
    ssid_buf[i] = c;
  }
  core::ssid = ssid_buf;

  int plen_addr = EEPROM_CRED_START + 1 + slen;
  int plen = EEPROM.read(plen_addr);
  if (plen < 0 || plen > 64) plen = 0;
  char pass_buf[65] = {0};
  for (int i = 0; i < plen; i++) {
    char c = EEPROM.read(plen_addr + 1 + i);
    if (c == 0xFF || c == 0) break;
    pass_buf[i] = c;
  }
  core::password = pass_buf;
}

ICACHE_FLASH_ATTR void saveGeneralSettings() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = EEPROM_GENSET_START;
  if (core::genset.broadcast_port < 1024 || core::genset.broadcast_port > 65500)
    core::genset.broadcast_port = BROADCAST_PORT;
  if (core::genset.command_port < 1024 || core::genset.command_port > 65500)
    core::genset.command_port = COMMAND_PORT;
  if (core::genset.report_interval < 5000 || core::genset.report_interval > 600000)
    core::genset.report_interval = BROADCAST_INTERVAL;
  EEPROM.put(addr, core::genset.broadcast_port);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, core::genset.command_port);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, core::genset.report_interval);
  addr += sizeof(uint32_t);
  EEPROM.commit();
}

ICACHE_FLASH_ATTR void factoryReset() {
  EEPROM.begin(EEPROM_SIZE);

  // --- Relay state ---
  memset(EEPROM.getDataPtr() + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);

  // --- WiFi credentials ---
  memset(EEPROM.getDataPtr() + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);

  // --- Genset config: write defaults ---
  int addr = EEPROM_GENSET_START;
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  EEPROM.put(addr, def_broadcast);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, def_command);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, def_interval);
  addr += sizeof(uint32_t);

  // --- Calibration + rules: clear rest of EEPROM ---
  memset(EEPROM.getDataPtr() + addr, 0, EEPROM_SIZE - addr);

  EEPROM.commit();
  delay(100);
  RESET_MCU();
}

ICACHE_FLASH_ATTR void saveCredentials(const String &s, const String &p) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_CRED_START, s.length());
  for (int i = 0; i < s.length(); i++) EEPROM.write(EEPROM_CRED_START + 1 + i, s[i]);
  int offset = EEPROM_CRED_START + 1 + s.length();
  EEPROM.write(offset, p.length());
  for (int i = 0; i < p.length(); i++) EEPROM.write(offset + 1 + i, p[i]);
  EEPROM.commit();
}

ICACHE_FLASH_ATTR void handleGenSetSave() {
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
  server.send(200, "text/plain", "RESET");
  factoryReset();
}

void handleToggleApi() {
  addCorsHeaders();
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  uint32_t id = strtoul(server.arg("id").c_str(), nullptr, 10);
  sensors::handleToggle(id);
  server.send(200, "text/plain", "OK");
}

void handleDimmerApi() {
  addCorsHeaders();
  if (!server.hasArg("value") || !server.hasArg("id")) {
    server.send(400, "text/plain", "id and value required");
    return;
  }
  int value = server.arg("value").toInt();
  uint32_t id = strtoul(server.arg("id").c_str(), nullptr, 10);
  sensors::handleDimmer(id, value);
  server.send(200, "text/plain", "OK");
}

void handleLogs() {
  addCorsHeaders();
  server.send(200, "application/json", logger::getRecentLogsJson());
}

void handleOtaToggle() {
  addCorsHeaders();
  if (server.hasArg("enabled")) {
    bool enable = server.arg("enabled") == "1";
    core::setOtaEnabled(enable);
    server.send(200, "text/plain", enable ? "OK" : "OFF");
  } else {
    server.send(400, "text/plain", "enabled arg required");
  }
}

ICACHE_FLASH_ATTR void loadCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p;
    EEPROM.get(addr, p);
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
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    auto &c = sensors::calibrations[i];
    CalibrationPersist current = {};
    if (c.local && c.uid != 0) {
      current = makePersist(c);
    }
    CalibrationPersist stored;
    EEPROM.get(addr, stored);
    if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
      EEPROM.put(addr, current);
    }
  }
  EEPROM.commit();
}

ICACHE_FLASH_ATTR void saveCalibrationSlot(int index) {
  if (index < 0 || index >= MAX_PERSISTED_SENSORS) return;
  EEPROM.begin(EEPROM_SIZE);
  int addr = EEPROM_CALIB_START + index * sizeof(CalibrationPersist);
  auto &c = sensors::calibrations[index];
  CalibrationPersist current = {};
  if (c.local && c.uid != 0) {
    current = makePersist(c);
  }
  CalibrationPersist stored;
  EEPROM.get(addr, stored);
  if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
    EEPROM.put(addr, current);
    logger::sensorsf("Calib slot %d saved (uid:%u)", index, c.uid);
    EEPROM.commit();
  }
}

void handleDeleteRule() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= MAX_RULES) {
    server.send(400, "text/plain", "invalid id");
    return;
  }
  automations::deleteRule((uint8_t)id);
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
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  int id = server.arg("id").toInt();
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
      int sensor_id = token.toInt();
      if (sensor_id < 0 || sensor_id >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid sensor index");
        return;
      }
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
        cmp_val = t.toInt();
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
        th = t.toInt();
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
      int actuator_id = token.toInt();
      if (actuator_id < 0 || actuator_id >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid actuator index");
        return;
      }
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
        action = t.toInt();
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
        level = t.toInt();
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
    int time_s = server.arg("time_s").toInt();
    if (time_s < 0 || time_s > 86400) {
      server.send(400, "text/plain", "invalid time_s");
      return;
    }
    r.time_s = time_s;
    int ys = server.arg("year_start").toInt();
    int ms = server.arg("month_start").toInt();
    int ds = server.arg("day_start").toInt();
    int ye = server.arg("year_end").toInt();
    int me = server.arg("month_end").toInt();
    int de = server.arg("day_end").toInt();
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
    int interval = server.arg("interval").toInt();
    if (interval < 1000 || interval > 3600000) {
      server.send(400, "text/plain", "invalid interval");
      return;
    }
    r.interval_ms = interval;
  }
  // ================= DELAY / COOLDOWN =================
  r.delay_ms = server.arg("delay").toInt();
  r.cooldown_ms = server.arg("cooldown").toInt();
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
  addCorsHeaders();
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  uint32_t sensorUid = strtoul(server.arg("id").c_str(), nullptr, 10);
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
  server.begin();
}

}
