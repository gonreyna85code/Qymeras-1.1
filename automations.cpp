#include "automations.h"
#include "sensors.h"
#include <EEPROM.h>

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
      case ACT_TOGGLE:
        sensors::handleToggle(c.id);
        break;
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

  for (int i = 0; i < MAX_RULES; i++) {
    Rule &r = rules[i];
    RuleState &s = states[i];

    if (r.sensor_count == 0 && r.actuator_count == 0) continue;

    bool trigger = r.logical_and ? true : false;

    // --- SENSORES (EDGE / THRESHOLD) ---
    if (r.type == RULE_EDGE || r.type == RULE_THRESHOLD) {
      for (int j = 0; j < r.sensor_count; j++) {
        if (r.sensor_idxs[j] >= MAX_SENSORS) continue;
        auto &sensor = sensors::calibrations[r.sensor_idxs[j]];

        bool val_trigger = false;

        if (r.type == RULE_EDGE) {
          bool raw = sensor.state;

          // Anti-bounce
          if (raw == s.last[j]) {
            if (s.counter[j] < CONFIRM_READS) s.counter[j]++;
          } else {
            s.last[j] = raw;
            s.counter[j] = 1;
          }
          if (s.counter[j] < CONFIRM_READS) continue;

          bool rising = (!s.stable[j] && raw);
          bool falling = (s.stable[j] && !raw);

          switch (r.cmp[j]) {
            case CMP_GT: val_trigger = rising; break;
            case CMP_LT: val_trigger = falling; break;
            default: val_trigger = false;
          }
          s.stable[j] = raw;
        } else if (r.type == RULE_THRESHOLD) {
          float val = sensor.value;
          bool condition_met = false;

          switch (r.cmp[j]) {
            case CMP_GT:
              condition_met = (val > r.threshold[j]);
              break;
            case CMP_LT:
              condition_met = (val < r.threshold[j]);
              break;
            case CMP_EQ:
              condition_met = (fabs(val - r.threshold[j]) < 0.5f);
              break;
            default:
              condition_met = false;
          }
          val_trigger = condition_met;
        }
        if (r.logical_and) trigger &= val_trigger;
        else trigger |= val_trigger;
      }
    }

    // --- TIME ---
    else if (r.type == RULE_TIME) {
      uint16_t timeOfDay = sensors::getMinutesOfDay();
      uint16_t ruleTime = r.time_s / 60;

      if (timeOfDay >= ruleTime && timeOfDay < ruleTime + 1) {
        if (s.last_time_exec != sensors::getTime().day) {
          s.last_time_exec = sensors::getTime().day;
          trigger = true;
        }
      }
    }

    // --- INTERVAL ---
    else if (r.type == RULE_INTERVAL) {
      if (now_ms - s.last_interval_exec < r.interval_ms) continue;
      s.last_interval_exec = now_ms;
      trigger = true;
    }

    if (!trigger) continue;
    if (now_ms - s.last_action < r.cooldown_ms) continue;

    // Ejecutar acciones
    if (r.delay_ms == 0) {
      executeActions(r, now_ms);
      s.last_action = now_ms;
    } else {
      if (!s.pending) {
        s.pending = true;
        s.trigger_time = now_ms;
      }
    }

    // Ejecutar acciones pendientes
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
    memset(states, 0, sizeof(states));
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
  loadRulesFromEEPROM();
  for (int i = 0; i < MAX_RULES; i++) {
    memset(&states[i], 0, sizeof(RuleState));
  }
}

}  // namespace automations