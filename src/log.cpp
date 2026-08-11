#include "log.h"
#include "mesh.h"
#include <stdio.h>
#include <stdarg.h>

namespace logger {

// ================= STATE =================

static bool serial_enabled = true;
static bool layer_enabled[3] = {true, true, true};
static Level min_level = INFO;

struct LogEntry {
  Layer layer;
  Level level;
  uint32_t timestamp;
  char message[MAX_LOG_MSG];
};

static LogEntry buffer[LOG_BUFFER_SIZE];
static uint8_t buffer_head = 0;
static uint8_t buffer_count = 0;

// ================= LAYER / LEVEL NAMES =================

static const char *layer_name(Layer l) {
  switch (l) {
    case CORE:    return "CORE";
    case SENSORS: return "SENS";
    case EVENTS:  return "EVNT";
    default:      return "???";
  }
}

static const char *level_name(Level lv) {
  switch (lv) {
    case INFO:  return "INF";
    case WARN:  return "WRN";
    case ERROR: return "ERR";
    default:    return "???";
  }
}

// ================= INIT =================

void init() {
  serial_enabled = true;
  layer_enabled[CORE] = true;
  layer_enabled[SENSORS] = true;
  layer_enabled[EVENTS] = true;
  min_level = INFO;
  buffer_head = 0;
  buffer_count = 0;
}

// ================= SERIAL CONTROL =================

void setSerialEnabled(bool enabled) {
  serial_enabled = enabled;
}

bool isSerialEnabled() {
  return serial_enabled;
}

// ================= LAYER FILTER =================

void setLayerEnabled(Layer layer, bool enabled) {
  if (layer <= EVENTS) {
    layer_enabled[layer] = enabled;
  }
}

bool isLayerEnabled(Layer layer) {
  return layer <= EVENTS ? layer_enabled[layer] : false;
}

// ================= LEVEL FILTER =================

void setMinLevel(Level level) {
  min_level = level;
}

Level getMinLevel() {
  return min_level;
}

// ================= OUTPUT =================

static void output(Layer layer, Level level, const char *msg) {
  // Filter
  if (level < min_level) return;
  if (!layer_enabled[layer]) return;

  // --- Serial ---
  if (serial_enabled) {
    Serial.printf("[%lu][%s][%s] %s\n", millis(), layer_name(layer), level_name(level), msg);
  }

  // --- GUI buffer ---
  LogEntry &entry = buffer[buffer_head];
  entry.layer = layer;
  entry.level = level;
  entry.timestamp = millis();
  strncpy(entry.message, msg, MAX_LOG_MSG - 1);
  entry.message[MAX_LOG_MSG - 1] = '\0';

  buffer_head = (buffer_head + 1) % LOG_BUFFER_SIZE;
  if (buffer_count < LOG_BUFFER_SIZE) buffer_count++;

  // --- UDP broadcast ---
  mesh::sendLog(layer, level, msg);
}

// ================= LOG FUNCTIONS =================

void log(Layer layer, Level level, const char *msg) {
  output(layer, level, msg);
}

void log(Layer layer, Level level, const String &msg) {
  output(layer, level, msg.c_str());
}

void logf(Layer layer, Level level, const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(layer, level, buf);
}

// ================= CONVENIENCE =================

void core(const char *msg)    { output(CORE, INFO, msg); }
void core(const String &msg)  { output(CORE, INFO, msg.c_str()); }
void sensors(const char *msg) { output(SENSORS, INFO, msg); }
void sensors(const String &msg){ output(SENSORS, INFO, msg.c_str()); }
void event(const char *msg)   { output(EVENTS, INFO, msg); }
void event(const String &msg) { output(EVENTS, INFO, msg.c_str()); }

void coref(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, INFO, buf);
}

void sensorsf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(SENSORS, INFO, buf);
}

void eventf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(EVENTS, INFO, buf);
}

void warn(const char *msg)    { output(CORE, WARN, msg); }
void warn(const String &msg)  { output(CORE, WARN, msg.c_str()); }
void error(const char *msg)   { output(CORE, ERROR, msg); }
void error(const String &msg) { output(CORE, ERROR, msg.c_str()); }

void warnf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, WARN, buf);
}

void errorf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, ERROR, buf);
}

// ================= GUI ACCESS =================

String getRecentLogsJson() {
  String json = "[";
  uint8_t start = buffer_count < LOG_BUFFER_SIZE
    ? 0
    : buffer_head;

  for (uint8_t i = 0; i < buffer_count; i++) {
    uint8_t idx = (start + i) % LOG_BUFFER_SIZE;
    const LogEntry &e = buffer[idx];
    if (i) json += ",";
    json += "{\"t\":";
    json += e.timestamp;
    json += ",\"l\":\"";
    json += layer_name(e.layer);
    json += "\",\"v\":\"";
    json += level_name(e.level);
    json += "\",\"m\":\"";
    // Escape quotes in message
    for (const char *p = e.message; *p; p++) {
      if (*p == '"') json += "\\\"";
      else if (*p == '\\') json += "\\\\";
      else if (*p == '\n') json += "\\n";
      else json += *p;
    }
    json += "\"}";
  }
  json += "]";
  return json;
}

void clearBuffer() {
  buffer_head = 0;
  buffer_count = 0;
}

}  // namespace logger
