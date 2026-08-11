#include "ai.h"
#include "sensors.h"
#include "log.h"

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#define HTTP_CLIENT HTTPClient
#define AI_BEGIN(client, url) http.begin(client, url)
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

static State state = IDLE;
static Response last_response = {false, false, 0.0f, ""};
static char last_error[64] = "";

static unsigned long last_request = 0;
static char current_prompt[192] = "";

// ================= FORWARD DECLARATIONS =================
String performRequest();
void parseResponse(const String &response);
static const char* extractJsonString(const char *json, const char *key);

// ================= INIT =================

void init() {
  state = IDLE;
  last_response.valid = false;
  last_error[0] = '\0';
  logger::core("AI module initialized");
}

void setConfig(const Config &cfg) {
  config = cfg;
  logger::coref("AI config: provider=%d model=%s", cfg.provider, cfg.model);
}

const Config& getConfig() {
  return config;
}

// ================= REQUEST =================

void requestDigital(const char *prompt) {
  if (!config.enabled) return;
  if (state == REQUESTING) return;

  unsigned long now = millis();
  if (now - last_request < config.rate_limit_ms) return;

  strncpy(current_prompt, prompt, sizeof(current_prompt) - 1);
  current_prompt[sizeof(current_prompt) - 1] = '\0';
  last_request = now;
  state = REQUESTING;
  logger::sensorsf("AI request (digital): %s", prompt);
}

void requestAnalog(const char *prompt) {
  if (!config.enabled) return;
  if (state == REQUESTING) return;

  unsigned long now = millis();
  if (now - last_request < config.rate_limit_ms) return;

  strncpy(current_prompt, prompt, sizeof(current_prompt) - 1);
  current_prompt[sizeof(current_prompt) - 1] = '\0';
  last_request = now;
  state = REQUESTING;
  logger::sensorsf("AI request (analog): %s", prompt);
}

void requestRaw(const char *prompt) {
  requestAnalog(prompt);
}

// ================= PROCESS =================

void process() {
  if (state != REQUESTING) return;

  String response = performRequest();

  if (response.length() == 0) {
    state = ERROR;
    strncpy(last_error, "HTTP request failed", sizeof(last_error) - 1);
    logger::error("AI request failed");
    return;
  }

  state = PARSING;
  parseResponse(response);

  if (last_response.valid) {
    state = DONE;
    logger::sensorsf("AI response: %s", last_response.raw);
  } else {
    state = ERROR;
    logger::warn("AI response parse failed");
  }
}

// ================= HTTP =================

String performRequest() {
  String response = "";

  WiFiClientSecure client;
  client.setInsecure();

  HTTP_CLIENT http;
  if (!AI_BEGIN(client, config.endpoint)) return "";

  http.addHeader("Content-Type", "application/json");
  if (strlen(config.api_key) > 0) {
    String auth = "Bearer ";
    auth += config.api_key;
    http.addHeader("Authorization", auth);
  }
  http.setTimeout(config.timeout_ms);

  // Build JSON manually
  String body = "{\"model\":\"";
  body += config.model;
  body += "\",\"messages\":[{\"role\":\"user\",\"content\":\"";
  for (const char *p = current_prompt; *p; p++) {
    if (*p == '"') body += "\\\"";
    else if (*p == '\\') body += "\\\\";
    else if (*p == '\n') body += "\\n";
    else body += *p;
  }
  body += "\"}],\"max_tokens\":50,\"temperature\":0.1}";

  int httpCode = http.POST(body);

  if (httpCode == 200) {
    response = http.getString();
  } else {
    logger::errorf("AI HTTP error: %d", httpCode);
  }

  http.end();
  return response;
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

  static char buf[256];
  int i = 0;
  quote++;
  while (*quote && *quote != '"' && i < 255) {
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

void parseResponse(const String &response) {
  const char *json = response.c_str();

  // Try OpenAI format: choices[0].message.content
  const char *content = extractJsonString(json, "content");

  // Try Ollama format: response
  if (!content || strlen(content) == 0) {
    content = extractJsonString(json, "response");
  }

  if (!content || strlen(content) == 0) {
    last_response.valid = false;
    return;
  }

  // Store raw
  strncpy(last_response.raw, content, sizeof(last_response.raw) - 1);
  last_response.raw[sizeof(last_response.raw) - 1] = '\0';
  last_response.valid = true;

  // Parse as digital
  String val = String(content);
  val.trim();
  val.toLowerCase();

  if (val == "true" || val == "on" || val == "yes" || val == "1" || val == "high") {
    last_response.digital = true;
    last_response.analog = 1.0f;
    sensors::aidig("AI_STATE", true);
    return;
  }
  if (val == "false" || val == "off" || val == "no" || val == "0" || val == "low") {
    last_response.digital = true;
    last_response.analog = 0.0f;
    sensors::aidig("AI_STATE", false);
    return;
  }

  // Parse as analog
  if (val.length() > 0 && (isDigit(val[0]) || val[0] == '-' || val[0] == '.')) {
    last_response.digital = false;
    last_response.analog = val.toFloat();
    sensors::aiana("AI_VALUE", last_response.analog);
    return;
  }

  // Raw string only
  last_response.digital = false;
  last_response.analog = 0.0f;
}

// ================= STATE =================

State getState() { return state; }

const Response& getLastResponse() { return last_response; }

const char* getLastError() { return last_error; }

// ================= UTILS =================

void enable(bool enabled) {
  config.enabled = enabled;
  logger::coref("AI %s", enabled ? "enabled" : "disabled");
}

bool isEnabled() { return config.enabled; }

}  // namespace ai
