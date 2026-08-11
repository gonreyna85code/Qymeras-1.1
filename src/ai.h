#pragma once
#include <Arduino.h>
#include "config.h"

namespace ai {

// ================= PROVIDERS =================
enum Provider : uint8_t {
  PROVIDER_OPENAI = 0,
  PROVIDER_OLLAMA = 1,
  PROVIDER_CUSTOM = 2
};

// ================= STATE =================
enum State : uint8_t {
  IDLE = 0,
  REQUESTING = 1,
  PARSING = 2,
  DONE = 3,
  ERROR = 4
};

// ================= CONFIG =================
struct Config {
  Provider provider;
  char endpoint[128];
  char api_key[64];
  char model[32];
  uint16_t timeout_ms;
  uint32_t rate_limit_ms;
  bool enabled;
};

// ================= RESPONSE =================
struct Response {
  bool valid;
  bool digital;
  float analog;
  char raw[256];
};

// ================= INIT =================
void init();
void setConfig(const Config &cfg);
const Config& getConfig();

// ================= REQUEST =================
void requestDigital(const char *prompt);
void requestAnalog(const char *prompt);
void requestRaw(const char *prompt);

// ================= PROCESS =================
void process();

// ================= STATE =================
State getState();
const Response& getLastResponse();
const char* getLastError();

// ================= UTILS =================
void enable(bool enabled);
bool isEnabled();

}  // namespace ai
