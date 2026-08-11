#pragma once
#include <Arduino.h>
#include <functional>
#include "config.h"

namespace core {
void begin();
void loop();

struct GeneralSettings {
  uint16_t broadcast_port;
  uint16_t command_port;
  uint32_t report_interval;
};
extern GeneralSettings genset;
extern String ssid, password;

bool is_connected();
}

void initSatellite();
void report();
void onCommandHook(uint32_t uid, uint8_t type, int value, bool state);


//void onCommand(JsonObject &data);
