#pragma once
#include <Arduino.h>
#include "config.h"

namespace ai {

// ================= PROVIDERS =================
// Provider-agnostic by design: the firmware only speaks plain HTTP/JSON to a
// user-configured endpoint. No vendor SDKs.
enum Provider : uint8_t {
  PROVIDER_OPENAI = 0,
  PROVIDER_OLLAMA = 1,
  PROVIDER_CUSTOM = 2
};

// ================= OUTPUT TYPES =================
enum OutType : uint8_t {
  OUT_DIGITAL = 0,   // strict "true"/"false" -> virtual digital sensor
  OUT_ANALOG = 1,    // validated number within [min,max] -> virtual analog sensor
  OUT_ANALYTIC = 2,  // free-form text result, stored + logged for later consumers
  OUT_CONTROL = 3    // reserved: LLM tool-calling into Qymera API (not implemented)
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
// Global transport/provider settings shared by all prompts.
struct Config {
  Provider provider;
  char endpoint[128];
  char api_key[64];
  char model[32];
  uint16_t timeout_ms;
  uint32_t rate_limit_ms;
  bool enabled;
};

// Per-prompt configuration (fixed pool of AI_MAX_PROMPTS slots).
// NOTE: the prompt TEXT is not mirrored into RAM — it lives only in the
// persisted config block (see storage::getAiPromptText) and is staged into a
// single buffer while a request runs. This keeps the ESP8266 RAM footprint
// minimal; the EEPROM class already buffers storage internally.
struct PromptCfg {
  bool enabled;
  OutType out_type;
  char name[17];       // entity name for the virtual sensor (16 chars max)
  char model[32];      // per-slot model override; empty = use global model
  float analog_min;    // ANALOG acceptance range (inclusive)
  float analog_max;
  uint32_t interval_ms; // automatic re-run period; 0 = manual/API runs only
};

// Last validated result of one slot.
struct SlotResult {
  bool valid;
  bool digital;
  float analog;
  unsigned long ts;    // millis() of last valid result
  char raw[128];        // raw content (ANALYTIC/CONTROL payload / diagnostics)
};

// ================= INIT =================
void init();

// Global provider/transport config.
void setConfig(const Config &cfg);
const Config& getConfig();
void enable(bool enabled);
bool isEnabled();

// Per-prompt configuration. setPrompt validates and normalizes (strings are
// truncated to fit, analog range sanity-checked); returns false on bad slot.
bool setPrompt(uint8_t idx, const PromptCfg &cfg);
const PromptCfg& getPrompt(uint8_t idx);

// Prompt text staging: called by the web layer (persisted via storage) and
// internally at request time. Text lives in storage, not in a runtime mirror.
void stagePromptText(const char *text);   // copy text into the run buffer
const char* stagedPromptText();           // valid between stage and completion

// True if global enabled AND at least one slot enabled. Drives opt-in gating.
bool anyEnabled();

// ================= RUN =================
// Queue a manual run. Returns false (no side effects) when: bad slot,
// AI globally disabled, slot disabled, out_type OUT_CONTROL (tool-calling
// interface not implemented yet), or another request already in flight.
bool runPrompt(uint8_t idx);

// Pump the state machine. Call once per loop(); internally gated:
// does nothing when !isEnabled() or no slots enabled (zero traffic).
void tick(unsigned long now);

void process();  // executes the in-flight request (blocking HTTP)

// ================= STATUS =================
State getState();
int8_t getActiveSlot();
const SlotResult& getSlotResult(uint8_t idx);
const char* getLastError();

// Stateless chat-proxy: forwards an arbitrary OpenAI-style JSON request body
// to the configured endpoint (adds stored API key) and returns the upstream
// response. Holds no conversation state (the browser drives the tool loop).
String chatProxy(const String &payload, int &httpCode);

}  // namespace ai
