#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <functional>
#include "config.h"

namespace core {
extern WebServerCompat server;

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
String get_uid();

struct ReportEntry {
  uint32_t uid;
  float value;
  float raw;
  bool state;
};
extern ReportEntry reports[MAX_SENSORS];
void setReport(uint8_t index, uint32_t uid, float value, float raw, bool state);

#pragma pack(push, 1)
struct PacketHeader {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
};
struct Packet {
  uint8_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
};
#pragma pack(pop)
}
void initSatellite();
void report();
void onCommandHook(uint32_t uid, uint8_t type, int value, bool state);


//void onCommand(JsonObject &data);
