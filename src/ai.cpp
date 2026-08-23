#include "ai.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "config.h"
#include "mesh.h"
#include "sensors.h"
#include "log.h"

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#define HTTP_CLIENT HTTPClient
#elif defined(ESP32)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#define HTTP_CLIENT HTTPClient
#define AI_BEGIN(client, url) http.begin(url)
#endif

namespace ai {

// ================= STATE =================

static Config config = {
  PROVIDER_OPENAI,
  "https://api.openai.com/v1/chat/completions",
  "",
  "gpt-4o-mini",
  10000,
  5000,
  false
};

static PromptCfg prompts[AI_MAX_PROMPTS];
static SlotResult results[AI_MAX_PROMPTS];
static unsigned long slot_last_run[AI_MAX_PROMPTS];

// Single staging buffer for the prompt text of the in-flight request.
static char staged_prompt[113] = "";

static State state = IDLE;
static int8_t active_slot = -1;
static unsigned long last_request = 0;
static char last_error[64] = "";

// ================= FORWARD DECLARATIONS =================
String performRequest(uint8_t slot);
static const char* extractJsonString(const char *json, const char *key);
static bool applyResult(uint8_t slot, const char *content);
static void startRun(uint8_t slot, unsigned long now);

// ================= INIT =================

void init() {
  state = IDLE;
  active_slot = -1;
  last_error[0] = '\0';
  memset(results, 0, sizeof(results));
  memset(slot_last_run, 0, sizeof(slot_last_run));
  logger::core("AI module initialized");
}

void setConfig(const Config &cfg) {
  config = cfg;
  logger::coref("AI config: provider=%d model=%s", cfg.provider, cfg.model);
}

const Config& getConfig() {
  return config;
}

// ================= PROMPTS =================

bool setPrompt(uint8_t idx, const PromptCfg &cfg) {
  if (idx >= AI_MAX_PROMPTS) return false;
  if (cfg.out_type > OUT_CONTROL) return false;
  if (!isfinite(cfg.analog_min) || !isfinite(cfg.analog_max)) return false;
  if (cfg.analog_min > cfg.analog_max) return false;

  PromptCfg &p = prompts[idx];
  memset(&p, 0, sizeof(PromptCfg));
  p.enabled = cfg.enabled;
  p.out_type = cfg.out_type;
  p.analog_min = cfg.analog_min;
  p.analog_max = cfg.analog_max;
  p.interval_ms = (cfg.interval_ms > 86400000UL) ? 86400000UL : cfg.interval_ms;

  strncpy(p.name, cfg.name, sizeof(p.name) - 1);
  p.name[sizeof(p.name) - 1] = '\0';
  if (p.name[0] == '\0') {
    snprintf(p.name, sizeof(p.name), "AI%u", (unsigned)idx);
  }

  strncpy(p.model, cfg.model, sizeof(p.model) - 1);
  p.model[sizeof(p.model) - 1] = '\0';

  return true;
}

void stagePromptText(const char *text) {
  if (!text) text = "";
  strncpy(staged_prompt, text, sizeof(staged_prompt) - 1);
  staged_prompt[sizeof(staged_prompt) - 1] = '\0';
}

const char* stagedPromptText() {
  return staged_prompt;
}

const PromptCfg& getPrompt(uint8_t idx) {
  return prompts[idx];
}

bool anyEnabled() {
  if (!config.enabled) return false;
  for (uint8_t i = 0; i < AI_MAX_PROMPTS; i++) {
    if (prompts[i].enabled && prompts[i].out_type != OUT_CONTROL) return true;
  }
  return false;
}

// ================= RUN =================

static void setError(const char *msg) {
  strncpy(last_error, msg, sizeof(last_error) - 1);
  last_error[sizeof(last_error) - 1] = '\0';
}

static void startRun(uint8_t slot, unsigned long now) {
  active_slot = (int8_t)slot;
  last_request = now;
  state = REQUESTING;
  logger::sensorsf("AI request (%s): %s",
                   prompts[slot].out_type == OUT_DIGITAL ? "digital" :
                   prompts[slot].out_type == OUT_ANALOG ? "analog" : "analytic",
                   staged_prompt);
}

bool runPrompt(uint8_t idx) {
  if (idx >= AI_MAX_PROMPTS) {
    setError("bad slot");
    return false;
  }
  if (!config.enabled) {
    setError("AI globally disabled");
    return false;
  }
  if (!prompts[idx].enabled) {
    setError("prompt slot disabled");
    return false;
  }
  if (state == REQUESTING) {
    setError("request already in flight");
    return false;
  }
  unsigned long now = millis();
  if (last_request != 0 && now - last_request < config.rate_limit_ms) {
    setError("rate limit");
    return false;
  }
  last_error[0] = '\0';
  startRun(idx, now);
  return true;
}

// Interval scheduling + request pump. Fully opt-in: no-op unless the global
// flag is on AND at least one non-CONTROL slot is enabled.
void tick(unsigned long now) {
  if (state == REQUESTING) {
    process();
    return;
  }
  if (!anyEnabled()) return;

  for (uint8_t i = 0; i < AI_MAX_PROMPTS; i++) {
    const PromptCfg &p = prompts[i];
    if (!p.enabled || p.out_type == OUT_CONTROL || p.interval_ms == 0) continue;
    if (now - slot_last_run[i] < p.interval_ms) continue;
    if (now - last_request < config.rate_limit_ms) return;
    last_error[0] = '\0';
    startRun(i, now);
    break;  // one request at a time
  }
}

// ================= PROCESS =================

void process() {
  if (state != REQUESTING || active_slot < 0) return;
  uint8_t slot = (uint8_t)active_slot;

  String response = performRequest(slot);

  if (response.length() == 0) {
    state = ERROR;
    setError("HTTP request failed");
    logger::error("AI request failed");
    slot_last_run[slot] = millis();
    active_slot = -1;
    return;
  }

  const char *json = response.c_str();
  const char *content = extractJsonString(json, "content");     // OpenAI format
  if (!content || strlen(content) == 0) {
    content = extractJsonString(json, "response");              // Ollama format
  }

  if (!content || strlen(content) == 0) {
    state = ERROR;
    setError("empty response content");
    logger::warn("AI response parse failed");
  } else if (applyResult(slot, content)) {
    state = DONE;
    logger::sensorsf("AI response: %s", results[slot].raw);
  } else {
    state = ERROR;
    logger::warnf("AI invalid response: %s", results[slot].raw);
  }

  slot_last_run[slot] = millis();
  active_slot = -1;
}

// Validate the model's free-form content against the slot's declared output
// type. Invalid answers NEVER reach the virtual sensors.
static bool applyResult(uint8_t slot, const char *content) {
  SlotResult &r = results[slot];
  r.valid = false;
  r.digital = false;
  r.analog = 0.0f;
  r.ts = millis();
  strncpy(r.raw, content, sizeof(r.raw) - 1);
  r.raw[sizeof(r.raw) - 1] = '\0';

  String val = String(content);
  val.trim();
  val.toLowerCase();
  const PromptCfg &p = prompts[slot];

  if (p.out_type == OUT_DIGITAL) {
    // Tolerant prefix: "true"/"false" optionally followed by punctuation/words
    // (small models often append explanation like "False. The earth...").
    auto isTruePrefix = [&]() -> bool {
      if (val == "true") return true;
      if (val.startsWith("true ") || val.startsWith("true.") ||
          val.startsWith("true,") || val.startsWith("true;") ||
          val.startsWith("true:")) return true;
      return false;
    };
    auto isFalsePrefix = [&]() -> bool {
      if (val == "false") return true;
      if (val.startsWith("false ") || val.startsWith("false.") ||
          val.startsWith("false,") || val.startsWith("false;") ||
          val.startsWith("false:")) return true;
      return false;
    };
    if (isTruePrefix()) {
      r.valid = true;
      r.digital = true;
      r.analog = 1.0f;
      sensors::aidig(String(p.name), true);
      return true;
    }
    if (isFalsePrefix()) {
      r.valid = true;
      r.digital = false;
      r.analog = 0.0f;
      sensors::aidig(String(p.name), false);
      return true;
    }
    setError("invalid digital response (true/false required)");
    return false;
  }

  if (p.out_type == OUT_ANALOG) {
    // Tolerant numeric: extract leading number, ignore trailing unit/text
    // (e.g. "35.20C" from TEMP context). Must have at least one numeric char.
    char *end = nullptr;
    errno = 0;
    float v = strtof(val.c_str(), &end);
    if (end == val.c_str()) {
      setError("invalid analog response (number required)");
      return false;
    }
    if (errno == ERANGE || !isfinite(v)) {
      setError("analog overflow");
      return false;
    }
    if (v < p.analog_min || v > p.analog_max) {
      setError("analog out of range");
      return false;
    }
    r.valid = true;
    r.analog = v;
    sensors::aiana(String(p.name), v);
    return true;
  }

  if (p.out_type == OUT_CONTROL) {
    // CONTROL is interface-only: store + log, never drives actuators directly.
    r.valid = true;
    logger::eventf("AI control [%s]: %s", p.name, r.raw);
    return true;
  }

  // OUT_ANALYTIC: any non-empty content is the deliverable. Stored in raw[]
  // and logged; future consumers (notifications/events) read from here.
  r.valid = true;
  logger::eventf("AI analytic [%s]: %s", p.name, r.raw);
  return true;
}

// ================= HTTP =================

static const char* sensorTypeName(uint8_t t) {
  switch (t) {
    case sensors::SENSOR_LUMI: return "LUMI";
    case sensors::SENSOR_HUMI: return "HUMI";
    case sensors::SENSOR_TEMP: return "TEMP";
    case sensors::SENSOR_PRESS: return "PRESS";
    case sensors::SENSOR_LEVEL: return "LEVEL";
    case sensors::SENSOR_AIRQ: return "AIRQ";
    case sensors::SENSOR_RAIN: return "RAIN";
    case sensors::TYPE_DIMMER: return "DIMMER";
    case sensors::TYPE_RELAY: return "RELAY";
    case sensors::SENSOR_TIME: return "TIME";
    case sensors::SENSOR_GENERIC: return "GENERIC";
    case sensors::SENSOR_CONTACT: return "CONTACT";
    case sensors::SENSOR_AIDIG: return "AIDIG";
    case sensors::SENSOR_AIANA: return "AIANA";
    default: return "UNKNOWN";
  }
}

static void appendJsonEscaped(String &out, const char *s) {
  for (const char *q = s; *q; q++) {
    if (*q == '"') out += "\\\"";
    else if (*q == '\\') out += "\\\\";
    else if (*q == '\n') out += "\\n";
    else if (*q == '\r') out += "\\r";
    else if (*q == '\t') out += "\\t";
    else out += *q;
  }
}

static String buildBody(uint8_t slot) {
  const PromptCfg &p = prompts[slot];

  // System context: explain why sensor data is sent + full visible snapshot.
  String sys;
  sys.reserve(1024);
  sys += "You are the AI assistant for Qymeras IoT device. You receive real-time sensor data because the user's prompt refers to device state and you must use it to answer. Sensors are sent as NAME(TYPE)=value/state. Use them to evaluate the prompt.\nSensors:\n";
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!sensors::isEntryVisible(i)) continue;
    auto &c = sensors::calibrations[i];
    if (c.type == sensors::SENSOR_TIME) continue; // exclude Unix time, confuses LLM
    auto &r = mesh::reports[i];
    char vbuf[24];
    if (c.type == sensors::TYPE_RELAY || c.type == sensors::SENSOR_AIDIG ||
        c.type == sensors::SENSOR_CONTACT || c.type == sensors::SENSOR_RAIN) {
      // digital: show ON/OFF from state
      sys += c.name;
      sys += "(";
      sys += sensorTypeName(c.type);
      sys += ")=";
      sys += (r.state ? "ON" : "OFF");
    } else {
      float v = r.value;
      if (isnan(v) || isinf(v)) v = 0;
      dtostrf(v, 0, 2, vbuf);
      sys += c.name;
      sys += "(";
      sys += sensorTypeName(c.type);
      sys += ")=";
      sys += vbuf;
      if (c.type == sensors::SENSOR_TEMP) sys += "C";
      else if (c.type == sensors::SENSOR_HUMI) sys += "%";
      else if (c.type == sensors::SENSOR_PRESS) sys += "kPa";
    }
    sys += c.local ? " local" : " remote";
    sys += "\n";
    if (sys.length() > 900) break; // cap to keep JSON under ~1.5KB
  }
  sys += "\nUse the sensor data to answer the user's prompt. For overtemperature risk, base your answer on the TEMP sensor.\n";

  String body;
  body.reserve(1536);
  body += "{\"model\":\"";
  body += (p.model[0] != '\0') ? p.model : config.model;
  body += "\",\"messages\":[{\"role\":\"system\",\"content\":\"";
  appendJsonEscaped(body, sys.c_str());
  body += "\"},{\"role\":\"user\",\"content\":\"";
  appendJsonEscaped(body, staged_prompt);
  const char* suffix = "";
  int maxTokens = 100;
  if (p.out_type == OUT_DIGITAL) {
    suffix = "\\nRespond with only the word true or false. No other text, no explanation, no punctuation.";
    maxTokens = 3;
  } else if (p.out_type == OUT_ANALOG) {
    suffix = "\\nAnswer with only a number. No unit, no text.";
    maxTokens = 8;
  }
  if (*suffix) body += suffix;
  body += "\"}],\"max_tokens\":";
  body += String(maxTokens);
  body += ",\"temperature\":0.1}";
  return body;
}

static String aiPerform(HTTP_CLIENT &http, uint8_t slot) {
  http.addHeader("Content-Type", "application/json");
  if (strlen(config.api_key) > 0) {
    String auth = "Bearer ";
    auth += config.api_key;
    http.addHeader("Authorization", auth);
  }
  http.setTimeout(config.timeout_ms);

  int httpCode = http.POST(buildBody(slot));
  String response = "";
  if (httpCode == 200) {
    response = http.getString();
  } else {
    logger::errorf("AI HTTP error: %d", httpCode);
  }
  http.end();
  return response;
}

String performRequest(uint8_t slot) {
#if defined(ESP8266)
  // Plain HTTP only on ESP8266: the BearSSL TLS buffers cannot fit this
  // firmware's DRAM budget, and linking mbedtls would push statics over the
  // stability line (observed /calib corruption at <16KB boot heap).
  // LAN providers (Ollama etc.) work over plain HTTP; cloud TLS endpoints
  // remain available on the ESP32 build.
  if (strncmp(config.endpoint, "https://", 8) == 0) {
    setError("TLS not supported on ESP8266 (use HTTP endpoint)");
    logger::errorf("AI endpoint rejected: https:// unsupported here");
    return "";
  }
  WiFiClient client;
  HTTP_CLIENT http;
  if (!http.begin(client, config.endpoint)) return "";
  return aiPerform(http, slot);
#else
  HTTP_CLIENT http;
  if (!http.begin(config.endpoint)) return "";
  return aiPerform(http, slot);
#endif
}

// ================= PARSE JSON (manual) =================

static const char* extractJsonString(const char *json, const char *key) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);

  const char *key_pos = strstr(json, pattern);
  if (!key_pos) return nullptr;

  const char *colon = strchr(key_pos, ':');
  if (!colon) return nullptr;

  const char *quote = strchr(colon, '"');
  if (!quote) return nullptr;

  static char buf[128];
  int i = 0;
  quote++;
  while (*quote && *quote != '"' && i < 127) {
    if (*quote == '\\' && *(quote + 1)) {
      quote++;
      if (*quote == 'n') buf[i++] = '\n';
      else if (*quote == 't') buf[i++] = '\t';
      else if (*quote == '"') buf[i++] = '"';
      else if (*quote == '\\') buf[i++] = '\\';
      else buf[i++] = *quote;
    } else {
      buf[i++] = *quote;
    }
    quote++;
  }
  buf[i] = '\0';
  return buf;
}

// ================= STATUS =================

State getState() { return state; }

int8_t getActiveSlot() { return active_slot; }

const SlotResult& getSlotResult(uint8_t idx) {
  return results[idx];
}

const char* getLastError() { return last_error; }

// ================= UTILS =================

void enable(bool enabled) {
  config.enabled = enabled;
  logger::coref("AI %s", enabled ? "enabled" : "disabled");
}

bool isEnabled() { return config.enabled; }

}  // namespace ai
