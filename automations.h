#pragma once
#include "config.h"
#include <stdint.h>

namespace automations {

enum RuleType : uint8_t { RULE_EDGE,
                          RULE_THRESHOLD,
                          RULE_TIME,
                          RULE_INTERVAL };
enum Comparator : uint8_t { CMP_GT,
                            CMP_LT };
enum ActionType : uint8_t { ACT_ON,
                            ACT_OFF,
                            ACT_TOGGLE,
                            ACT_LEVEL };

struct Rule {
  uint8_t sensor_idxs[5];
  uint8_t sensor_count;
  RuleType type;
  Comparator cmp[5];
  int16_t threshold[5];
  bool logical_and;
  uint8_t actuator_idxs[5];
  ActionType actions[5];
  uint8_t levels[5];
  uint8_t actuator_count;
  uint16_t delay_ms;
  uint32_t cooldown_ms;
  uint32_t time_s;
  uint32_t interval_ms;
  uint16_t year_start;
  uint16_t year_end;
  uint8_t month_start;
  uint8_t month_end;
  uint8_t day_start;
  uint8_t day_end;
};

extern Rule rules[MAX_RULES];

void init();
void tick(uint32_t now_ms);
void saveRulesToEEPROM();
void deleteRule(uint8_t idx);

}