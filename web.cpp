#include "web.h"
#include "html.h"
#include "core.h"
#include "config.h"
#include "sensors.h"
#include "automations.h"

namespace web {

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
    core::server.sendContent_P(PSTR("let savedTab='wifi';show(savedTab);"));
  else
    core::server.sendContent_P(PSTR("let savedTab=(localStorage.getItem('tab')||'control');show(savedTab);"));
  core::server.sendContent_P(
    PSTR("['control','auto','config','wifi'].forEach(t=>{document.getElementById('t_'+t).onclick=()=>show(t);});"));
  core::server.sendContent_P(PSTR(
    "window.genset={"
    "broadcast_port:"));
  core::server.sendContent(String(core::genset.broadcast_port));
  core::server.sendContent_P(PSTR(
    ",command_port:"));
  core::server.sendContent(String(core::genset.command_port));
  core::server.sendContent_P(PSTR(
    ",report_interval:"));
  core::server.sendContent(String(core::genset.report_interval));
  core::server.sendContent_P(PSTR("};"));
}

ICACHE_FLASH_ATTR void handleRoot() {
  core::server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  core::server.send(200, "text/html", "");
  core::server.sendContent_P(html::HTML_PAGE_1);
  core::server.sendContent_P(html::HTML_PAGE_2);
  sendStartupJS();
  core::server.sendContent_P(html::HTML_PAGE_3);
  core::server.sendContent_P(html::HTML_PAGE_4);
  core::server.sendContent("");
}

ICACHE_FLASH_ATTR void handleSave() {
  saveCredentials(core::server.arg("ssid"), core::server.arg("pass"));
  core::server.sendHeader("Location", "/?saved=1");
  core::server.send(303);
  delay(1000);
  ESP.restart();
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
  core::ssid = "";
  for (int i = 0; i < slen; i++) {
    char c = EEPROM.read(EEPROM_CRED_START + 1 + i);
    if (c == 0xFF || c == 0) break;
    core::ssid += c;
  }
  int plen_addr = EEPROM_CRED_START + 1 + slen;
  int plen = EEPROM.read(plen_addr);
  if (plen < 0 || plen > 64) plen = 0;
  core::password = "";
  for (int i = 0; i < plen; i++) {
    char c = EEPROM.read(plen_addr + 1 + i);
    if (c == 0xFF || c == 0) break;
    core::password += c;
  }
}

ICACHE_FLASH_ATTR void saveGeneralSettings() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = EEPROM_GENSET_START;
  if (core::genset.broadcast_port < 1024 || core::genset.broadcast_port > 65500)
    core::genset.broadcast_port = BROADCAST_PORT;
  if (core::genset.command_port < 1024 || core::genset.command_port > 65500)
    core::genset.command_port = COMMAND_PORT;
  if (core::genset.report_interval < 10000 || core::genset.report_interval > 600000)  // max 10 min
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
  for (int i = 0; i < EEPROM_RELAY_STATE_SIZE; i++) {
    EEPROM.write(EEPROM_RELAY_STATE_START + i, 0);
  }
  for (int i = 0; i < EEPROM_CRED_SIZE; i++) {
    EEPROM.write(EEPROM_CRED_START + i, 0);
  }
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  EEPROM.put(addr, def_broadcast);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, def_command);
  addr += sizeof(uint16_t);
  EEPROM.put(addr, def_interval);
  addr += sizeof(uint32_t);
  for (int i = EEPROM_CALIB_START; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(300);
  ESP.restart();
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
  if (core::server.method() != HTTP_POST) {
    core::server.send(405, "text/plain", "POST required");
    return;
  }
  if (core::server.hasArg("broadcast"))
    core::genset.broadcast_port = core::server.arg("broadcast").toInt();
  if (core::server.hasArg("command"))
    core::genset.command_port = core::server.arg("command").toInt();
  if (core::server.hasArg("interval"))
    core::genset.report_interval = core::server.arg("interval").toInt();
  saveGeneralSettings();
  core::server.sendHeader("Location", "/");
  core::server.send(200, "text/plain", "OK");
}

ICACHE_FLASH_ATTR void handleFactoryReset() {
  core::server.send(200, "text/plain", "RESET");
  delay(200);
  factoryReset();
}

void handleToggleApi() {
  if (!core::server.hasArg("key")) {
    core::server.send(400, "text/plain", "key required");
    return;
  }
  sensors::handleToggle(core::server.arg("key"));
  core::server.send(200, "text/plain", "OK");
}

void handleDimmerApi() {
  if (!core::server.hasArg("value") || !core::server.hasArg("key")) {
    core::server.send(400, "text/plain", "value required");
    return;
  }
  int value = core::server.arg("value").toInt();
  sensors::handleDimmer(core::server.arg("key"), value);
  core::server.send(200, "text/plain", "OK");
}

ICACHE_FLASH_ATTR void loadCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p;
    EEPROM.get(addr, p);
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
    c.value = 0;
  }
}

ICACHE_FLASH_ATTR void saveCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist current = makePersist(sensors::calibrations[i]);
    CalibrationPersist stored;
    EEPROM.get(addr, stored);
    if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
      EEPROM.put(addr, current);
    }
  }
  EEPROM.commit();
}

void handleDeleteRule() {
  if (!core::server.hasArg("id")) {
    core::server.send(400, "text/plain", "missing id");
    return;
  }
  int id = core::server.arg("id").toInt();
  if (id < 0 || id >= MAX_RULES) {
    core::server.send(400, "text/plain", "invalid id");
    return;
  }
  automations::deleteRule((uint8_t)id);
  core::server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleRules() {
  if (core::server.method() != HTTP_GET) {
    core::server.send(405, "text/plain", "GET required");
    return;
  }
  core::server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  core::server.send(200, "application/json", "");
  core::server.sendContent("[");
  bool first = true;
  for (int i = 0; i < MAX_RULES; i++) {
    const automations::Rule &r = automations::rules[i];
    if (r.sensor_count == 0 && r.actuator_count == 0)
      continue;
    if (!first) core::server.sendContent(",");
    first = false;
    core::server.sendContent("{");
    core::server.sendContent("\"id\":");
    core::server.sendContent(String(i));
    // sensores
    core::server.sendContent(",\"sensors\":[");
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) core::server.sendContent(",");
      core::server.sendContent(String(r.sensor_idxs[s]));
    }
    core::server.sendContent("]");
    // tipo de regla
    core::server.sendContent(",\"type\":");
    core::server.sendContent(String(r.type));
    // lógica AND/OR
    core::server.sendContent(",\"logical_and\":");
    core::server.sendContent(String(r.logical_and));
    // comparadores
    core::server.sendContent(",\"cmp\":[");
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) core::server.sendContent(",");
      core::server.sendContent(String(r.cmp[s]));
    }
    core::server.sendContent("]");
    // thresholds
    core::server.sendContent(",\"threshold\":[");
    for (int s = 0; s < r.sensor_count; s++) {
      if (s) core::server.sendContent(",");
      core::server.sendContent(String(r.threshold[s]));
    }
    core::server.sendContent("]");
    // actuadores
    core::server.sendContent(",\"actuators\":[");
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) core::server.sendContent(",");
      core::server.sendContent(String(r.actuator_idxs[a]));
    }
    core::server.sendContent("]");
    // acciones
    core::server.sendContent(",\"actions\":[");
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) core::server.sendContent(",");
      core::server.sendContent(String(r.actions[a]));
    }
    core::server.sendContent("]");
    // niveles
    core::server.sendContent(",\"levels\":[");
    for (int a = 0; a < r.actuator_count; a++) {
      if (a) core::server.sendContent(",");
      core::server.sendContent(String(r.levels[a]));
    }
    core::server.sendContent("]");
    // timing
    core::server.sendContent(",\"delay_ms\":");
    core::server.sendContent(String(r.delay_ms));
    core::server.sendContent(",\"cooldown_ms\":");
    core::server.sendContent(String(r.cooldown_ms));
    core::server.sendContent(",\"time_s\":");
    core::server.sendContent(String(r.time_s));
    core::server.sendContent(",\"interval_ms\":");
    core::server.sendContent(String(r.interval_ms));
    // calendario
    core::server.sendContent(",\"year_start\":");
    core::server.sendContent(String(r.year_start));
    core::server.sendContent(",\"year_end\":");
    core::server.sendContent(String(r.year_end));
    core::server.sendContent(",\"month_start\":");
    core::server.sendContent(String(r.month_start));
    core::server.sendContent(",\"month_end\":");
    core::server.sendContent(String(r.month_end));
    core::server.sendContent(",\"day_start\":");
    core::server.sendContent(String(r.day_start));
    core::server.sendContent(",\"day_end\":");
    core::server.sendContent(String(r.day_end));
    core::server.sendContent("}");
  }
  core::server.sendContent("]");
  core::server.sendContent("");
}

void handleSetRule() {

  using namespace automations;

  if (!core::server.hasArg("id")) {
    core::server.send(400, "text/plain", "missing id");
    return;
  }

  int id = core::server.arg("id").toInt();

  if (id < 0) {
    for (int i = 0; i < MAX_RULES; i++) {
      if (rules[i].sensor_count == 0 && rules[i].actuator_count == 0) {
        id = i;
        break;
      }
    }
  }

  if (id < 0 || id >= MAX_RULES) {
    core::server.send(400, "text/plain", "invalid id");
    return;
  }

  Rule &r = rules[id];
  memset(&r, 0, sizeof(Rule));

  // ================= TYPE =================
  if (!core::server.hasArg("type")) {
    core::server.send(400, "text/plain", "type required");
    return;
  }

  int ruleType = core::server.arg("type").toInt();
  if (ruleType < 0 || ruleType > 3) {
    core::server.send(400, "text/plain", "invalid type");
    return;
  }

  r.type = (RuleType)ruleType;

  // ================= SENSORS =================
  if (core::server.hasArg("sensors")) {

    String sensors_str = core::server.arg("sensors");
    String cmp_str = core::server.arg("cmp");
    String threshold_str = core::server.arg("threshold");

    int idx = 0;

    while (sensors_str.length() && idx < 5) {
      int comma = sensors_str.indexOf(',');
      String token = (comma == -1) ? sensors_str : sensors_str.substring(0, comma);

      int sensor_id = token.toInt();

      if (sensor_id < 0 || sensor_id >= MAX_SENSORS) {
        core::server.send(400, "text/plain", "invalid sensor index");
        return;
      }

      if (sensors::calibrations[sensor_id].uid == 0) {
        core::server.send(400, "text/plain", "sensor not configured");
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
          core::server.send(400, "text/plain", "invalid comparator");
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
          core::server.send(400, "text/plain", "threshold out of range");
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
  if (core::server.hasArg("actuators")) {

    String actuators_str = core::server.arg("actuators");
    String actions_str = core::server.arg("actions");
    String levels_str = core::server.arg("levels");

    int idx = 0;

    while (actuators_str.length() && idx < 5) {

      int comma = actuators_str.indexOf(',');
      String token = (comma == -1) ? actuators_str : actuators_str.substring(0, comma);

      int actuator_id = token.toInt();

      if (actuator_id < 0 || actuator_id >= MAX_SENSORS) {
        core::server.send(400, "text/plain", "invalid actuator index");
        return;
      }

      auto &cal = sensors::calibrations[actuator_id];

      if (cal.uid == 0) {
        core::server.send(400, "text/plain", "actuator not configured");
        return;
      }

      if (cal.type != sensors::TYPE_RELAY && cal.type != sensors::TYPE_DIMMER) {
        core::server.send(400, "text/plain", "invalid actuator type");
        return;
      }

      r.actuator_idxs[idx] = actuator_id;

      int action = 2;
      if (actions_str.length()) {
        int c = actions_str.indexOf(',');
        String t = (c == -1) ? actions_str : actions_str.substring(0, c);
        action = t.toInt();

        if (action < 0 || action > 3) {
          core::server.send(400, "text/plain", "invalid action");
          return;
        }

        if (action == ACT_LEVEL && cal.type != sensors::TYPE_DIMMER) {
          core::server.send(400, "text/plain", "LEVEL only for dimmers");
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
          core::server.send(400, "text/plain", "level out of range");
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
    core::server.send(400, "text/plain", "at least one actuator required");
    return;
  }

  if ((r.type == RULE_EDGE || r.type == RULE_THRESHOLD) && r.sensor_count == 0) {
    core::server.send(400, "text/plain", "sensors required");
    return;
  }

  // ================= TIME =================
  if (r.type == RULE_TIME) {

    int time_s = core::server.arg("time_s").toInt();
    if (time_s < 0 || time_s > 86400) {
      core::server.send(400, "text/plain", "invalid time_s");
      return;
    }
    r.time_s = time_s;

    int ys = core::server.arg("year_start").toInt();
    int ms = core::server.arg("month_start").toInt();
    int ds = core::server.arg("day_start").toInt();

    int ye = core::server.arg("year_end").toInt();
    int me = core::server.arg("month_end").toInt();
    int de = core::server.arg("day_end").toInt();

    bool hasDate = ys || ms || ds || ye || me || de;

    if (hasDate) {

      if (ys && (ys < 1970 || ys > 2100)) {
        core::server.send(400, "text/plain", "invalid year_start");
        return;
      }
      if (ye && (ye < 1970 || ye > 2100)) {
        core::server.send(400, "text/plain", "invalid year_end");
        return;
      }

      if (ms && (ms < 1 || ms > 12)) {
        core::server.send(400, "text/plain", "invalid month_start");
        return;
      }
      if (me && (me < 1 || me > 12)) {
        core::server.send(400, "text/plain", "invalid month_end");
        return;
      }

      if (ds && (ds < 1 || ds > 31)) {
        core::server.send(400, "text/plain", "invalid day_start");
        return;
      }
      if (de && (de < 1 || de > 31)) {
        core::server.send(400, "text/plain", "invalid day_end");
        return;
      }

      r.year_start = ys;
      r.month_start = ms;
      r.day_start = ds;

      r.year_end = ye;
      r.month_end = me;
      r.day_end = de;

      // coherencia básica
      if (ys && ye && ys > ye) {
        core::server.send(400, "text/plain", "start > end");
        return;
      }
    }
  }

  // ================= INTERVAL =================
  if (r.type == RULE_INTERVAL) {
    int interval = core::server.arg("interval").toInt();
    if (interval < 1000 || interval > 3600000) {
      core::server.send(400, "text/plain", "invalid interval");
      return;
    }
    r.interval_ms = interval;
  }

  // ================= DELAY / COOLDOWN =================

  r.delay_ms = core::server.arg("delay").toInt();
  r.cooldown_ms = core::server.arg("cooldown").toInt();

  saveRulesToEEPROM();
  core::server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleCalib() {
  if (core::server.method() != HTTP_GET) {
    core::server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  core::server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  core::server.send(200, "application/json", "");
  core::server.sendContent("[");
  bool firstObj = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    auto &r = core::reports[i];
    if (c.uid == 0) continue;
    if (!firstObj) core::server.sendContent(",");
    firstObj = false;
    core::server.sendContent("{");
    core::server.sendContent("\"id\":");
    core::server.sendContent(String(i));
    core::server.sendContent(",\"name\":\"");
    core::server.sendContent(c.id);
    core::server.sendContent("\"");
    core::server.sendContent(",\"value\":");
    char buf[24];
    dtostrf(r.value, 0, 4, buf);
    core::server.sendContent(buf);
#define SEND_INT(name, val) \
  core::server.sendContent(",\"" name "\":"); \
  core::server.sendContent(String(val));
#define SEND_BOOL(name, val) \
  core::server.sendContent(",\"" name "\":"); \
  core::server.sendContent((val) ? "true" : "false");
    SEND_BOOL("pers_state", c.pers_state);
    SEND_INT("min", c.min);
    SEND_INT("max", c.max);
    SEND_INT("correction", c.correction);
    SEND_INT("avail", c.avail);
    SEND_BOOL("pulse", c.pulse);
    SEND_BOOL("state", r.state);
    SEND_INT("pulse_ms", c.pulse_ms);
    SEND_BOOL("persist", c.persist);
    SEND_INT("fade", c.fade);
    SEND_INT("type", c.type);
    SEND_INT("pin", c.pin);
#undef SEND_INT
#undef SEND_BOOL
    core::server.sendContent("}");
  }
  core::server.sendContent("]");
}


ICACHE_FLASH_ATTR void handleCalibSet() {
  if (core::server.method() != HTTP_POST) {
    core::server.send(405, "text/plain", "POST required");
    return;
  }
  String sensorName = core::server.arg("name");
  String type = core::server.arg("type");
  int calibIdx = -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors::calibrations[i].id == sensorName) {
      calibIdx = i;
      break;
    }
  }
  if (calibIdx < 0) {
    core::server.send(400, "text/plain", "Sensor not found");
    return;
  }
  auto &c = sensors::calibrations[calibIdx];
  auto &r = core::reports[calibIdx];
  float raw = r.raw;
  float ref = core::server.hasArg("ref") ? core::server.arg("ref").toFloat() : raw;
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
    bool enable = core::server.arg("ref") == "1";
    c.persist = enable;
    c.pulse = false;
  } else if (type == "avail") {
    c.avail = ref ? 1 : 0;
  } else if (type == "res") {
    c.min = 0;
    c.max = 100;
    c.correction = 0;
  } else {
    core::server.send(400, "text/plain", "Bad type");
    return;
  }
  saveCalibration();
  core::server.send(200, "text/plain", "OK");
}

}
