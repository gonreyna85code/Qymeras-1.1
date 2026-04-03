#include "automations.h"
#include "sensors.h"
#include <EEPROM.h>

#ifdef USE_RTC
#include "RTClib.h"
RTC_DS3231 rtc;
#endif

namespace automations {

struct RulesHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
};

static const uint32_t RULES_MAGIC = 0x4155544F;  // "AUTO"
static const uint16_t RULES_VERSION = 1;

static const uint8_t CONFIRM_READS = 3;
static const uint32_t SAMPLE_MS = 50;
static uint32_t last_run = 0;

// ----------------- REGLAS -----------------
struct RuleState {
  bool stable[5];
  bool last[5];
  uint8_t counter[5];
  bool pending;
  uint32_t trigger_time;
  uint32_t last_action;
  uint32_t last_time_exec;
  uint32_t last_interval_exec;
};

Rule rules[MAX_RULES];
static RuleState states[MAX_RULES];

// ----------------- ACCIONES -----------------
static void executeActions(const Rule &r, uint32_t now_ms) {
  for (int i = 0; i < r.actuator_count; i++) {
    if (r.actuator_idxs[i] >= MAX_SENSORS) continue;
    auto &c = sensors::calibrations[r.actuator_idxs[i]];
    switch (r.actions[i]) {
      case ACT_ON:
        if (!c.state) sensors::handleToggle(c.id);
        break;
      case ACT_OFF:
        if (c.state) sensors::handleToggle(c.id);
        break;
      case ACT_TOGGLE: sensors::handleToggle(c.id); break;
      case ACT_LEVEL:
        if (c.type == sensors::TYPE_DIMMER) sensors::handleDimmer(c.id, r.levels[i]);
        break;
    }
  }
}

// ----------------- TICK -----------------
void tick(uint32_t now_ms) {
  if (now_ms - last_run < SAMPLE_MS) return;
  last_run = now_ms;
  uint32_t now_time = millis() / 1000;

#ifdef USE_RTC
  DateTime now = rtc.now();
  uint16_t rtc_year = now.year();
  uint8_t rtc_month = now.month();
  uint8_t rtc_day = now.day();
#endif

  for (int i = 0; i < MAX_RULES; i++) {
    Rule &r = rules[i];
    RuleState &s = states[i];
    bool trigger = r.logical_and ? true : false;

    // --- CALENDARIO (RTC opcional) ---
#ifdef USE_RTC
    if (r.year_start && (rtc_year < r.year_start || rtc_year > r.year_end)) continue;
    if (r.month_start && (rtc_month < r.month_start || rtc_month > r.month_end)) continue;
    if (r.day_start && (rtc_day < r.day_start || rtc_day > r.day_end)) continue;
#endif

    // --- SENSORES (EDGE / THRESHOLD) ---
    if (r.type == RULE_EDGE || r.type == RULE_THRESHOLD) {
      for (int j = 0; j < r.sensor_count; j++) {
        if (r.sensor_idxs[j] >= MAX_SENSORS) continue;
        auto &sensor = sensors::calibrations[r.sensor_idxs[j]];

        // Anti-bounce
        bool raw = sensor.state;
        if (raw == s.last[j]) {
          if (s.counter[j] < CONFIRM_READS) s.counter[j]++;
        } else {
          s.last[j] = raw;
          s.counter[j] = 1;
        }
        if (s.counter[j] < CONFIRM_READS) continue;

        bool val_trigger = false;
        if (r.type == RULE_EDGE) {
          bool rising = (!s.stable[j] && raw);
          bool falling = (s.stable[j] && !raw);
          switch (r.cmp[j]) {  // EDGE: CMP_GT=Rising, CMP_LT=Falling
            case CMP_GT: val_trigger = rising; break;
            case CMP_LT: val_trigger = falling; break;
          }
          s.stable[j] = raw;
        } else if (r.type == RULE_THRESHOLD) {
          float val = sensor.value;
          val_trigger = (r.cmp[j] == CMP_GT) ? (val > r.threshold[j])
                                             : (val < r.threshold[j]);
          if (val_trigger == s.stable[j]) val_trigger = false;
          s.stable[j] = val_trigger;
        }

        if (r.logical_and) trigger &= val_trigger;
        else trigger |= val_trigger;
      }
    }

    // --- TIME ---
    else if (r.type == RULE_TIME) {
      if (now_time < r.time_s) continue;
      if (s.last_time_exec == now_time) continue;
      s.last_time_exec = now_time;
      trigger = true;
    }

    // --- INTERVAL ---
    else if (r.type == RULE_INTERVAL) {
      if (now_ms - s.last_interval_exec < r.interval_ms) continue;
      s.last_interval_exec = now_ms;
      trigger = true;
    }

    if (!trigger) continue;
    if (now_ms - s.last_action < r.cooldown_ms) continue;

    if (r.delay_ms == 0) {
      executeActions(r, now_ms);
      s.last_action = now_ms;
    } else {
      s.pending = true;
      s.trigger_time = now_ms;
    }

    if (s.pending && now_ms - s.trigger_time >= r.delay_ms) {
      executeActions(r, now_ms);
      s.pending = false;
      s.last_action = now_ms;
    }
  }
}

void loadRulesFromEEPROM() {
  RulesHeader h;
  int addr = EEPROM_RULES_START;
  EEPROM.get(addr, h);
  addr += sizeof(RulesHeader);
  if (h.magic != RULES_MAGIC || h.version != RULES_VERSION) {
    memset(rules, 0, sizeof(rules));
    return;
  }
  for (int i = 0; i < MAX_RULES; i++) {
    EEPROM.get(addr, rules[i]);
    addr += sizeof(Rule);
  }
}

void saveRulesToEEPROM() {
  RulesHeader h;
  h.magic = RULES_MAGIC;
  h.version = RULES_VERSION;
  h.count = MAX_RULES;
  int addr = EEPROM_RULES_START;
  EEPROM.put(addr, h);
  addr += sizeof(RulesHeader);
  for (int i = 0; i < MAX_RULES; i++) {
    EEPROM.put(addr, rules[i]);
    addr += sizeof(Rule);
  }
  EEPROM.commit();
}

void deleteRule(uint8_t idx) {
  if (idx >= MAX_RULES) return;
  memset(&rules[idx], 0, sizeof(Rule));
  memset(&states[idx], 0, sizeof(RuleState));
  saveRulesToEEPROM();
}

// ----------------- INIT -----------------
void init() {
#ifdef USE_RTC
  rtc.begin();  // inicializar RTC si se usa
#endif
  loadRulesFromEEPROM();
  for (int i = 0; i < MAX_RULES; i++) {
    memset(&states[i], 0, sizeof(RuleState));
  }
}

// ----------------- EEPROM -----------------


}  // namespace automations