#include "config.h"
#include "sensors.h"
#include "core.h"
#include "mesh.h"
#include <WiFiClient.h>
#include <time.h>

namespace sensors {

Calibration calibrations[MAX_SENSORS];
Fade activeFades[MAX_SENSORS];

static time_t time_offset = 0;
static TimeSource time_source = TIME_NONE;

void init() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    memset(&calibrations[i], 0, sizeof(Calibration));
    calibrations[i].local = true;
  }
  
  // Registrar callbacks de mesh
  mesh::setSensorDiscoveryCallback(onRemoteSensorDiscovered);
  mesh::setCommandCallback(onRemoteCommand);
}

void applyPersistedStates() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == 0) continue;
    if (calibrations[i].type != TYPE_RELAY) continue;
    if (!calibrations[i].persist) continue;
    if (calibrations[i].local) {
      pinMode(calibrations[i].pin, OUTPUT);
      digitalWrite(calibrations[i].pin, calibrations[i].pers_state ? HIGH : LOW);
    }
  }
}

void applyFades() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activeFades[i].active) continue;
    if (!calibrations[i].local) continue;
    
    unsigned long elapsed = millis() - activeFades[i].startTime;
    if (elapsed >= activeFades[i].duration) {
      analogWrite(activeFades[i].pin, activeFades[i].endVal);
      activeFades[i].active = false;
    } else {
      float progress = (float)elapsed / activeFades[i].duration;
      int current = activeFades[i].startVal + 
                    (activeFades[i].endVal - activeFades[i].startVal) * progress;
      analogWrite(activeFades[i].pin, current);
    }
  }
}

int findCalib(const String &key) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].id == key) return i;
  }
  return -1;
}

int findCalibByUid(uint32_t uid) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == uid) return i;
  }
  return -1;
}

void setRelay(const String &key, bool target) {
  int idx = findCalib(key);
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  if (c.type != TYPE_RELAY) return;
  
  if (c.local) {
    // Relé local
    c.state = target;
    if (c.pulse && target) {
      digitalWrite(c.pin, HIGH);
      delay(c.pulse_ms);
      digitalWrite(c.pin, LOW);
      c.state = false;
    } else {
      digitalWrite(c.pin, target ? HIGH : LOW);
    }
    if (c.persist) c.pers_state = target;
  } else {
    // Relé remoto
    handleRemoteActuator(c.device_uid, c.device_ip, c.id, target);
  }
}

void handleDimmer(const String &key, int value) {
  int idx = findCalib(key);
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  if (c.type != TYPE_DIMMER) return;
  
  value = constrain(value, 0, 100);
  
  if (c.local) {
    // Dimmer local
    int pwm_val = map(value, 0, 100, 0, 255);
    if (c.fade > 0) {
      startFade(key, c.pin, analogRead(c.pin), pwm_val, c.fade);
    } else {
      analogWrite(c.pin, pwm_val);
    }
    c.value = value;
    c.state = (value > 0);
  } else {
    // Dimmer remoto
    handleRemoteActuator(c.device_uid, c.device_ip, c.id, true, value);
  }
}

void handleToggle(const String &key) {
  int idx = findCalib(key);
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  if (c.type == TYPE_RELAY) {
    setRelay(key, !c.state);
  } else if (c.type == TYPE_DIMMER) {
    if (c.state) {
      handleDimmer(key, 0);
    } else {
      handleDimmer(key, 100);
    }
  }
}

void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur) {
  int idx = findCalib(key);
  if (idx < 0) return;
  
  activeFades[idx].pin = pin;
  activeFades[idx].startVal = from;
  activeFades[idx].endVal = to;
  activeFades[idx].startTime = millis();
  activeFades[idx].duration = dur;
  activeFades[idx].active = true;
}

void updateFades() {
  applyFades();
}

void temperature(const String &key, float raw) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_TEMP;
  c.local = true;
  c.value = calibrate(key, raw);
}

void humidity(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_HUMI;
  c.local = true;
  c.value = calibrate(key, raw);
}

void luminosity(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_LUMI;
  c.local = true;
  c.value = raw;
}

void level(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_LEVEL;
  c.local = true;
  c.value = calibrate(key, raw);
}

void pressure(const String &key, float raw) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_PRESS;
  c.local = true;
  c.value = calibrate(key, raw);
}

void airQ(const String &key, const int &v) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_AIRQ;
  c.local = true;
  c.value = v;
}

void rain(const String &key, bool v) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = SENSOR_RAIN;
  c.local = true;
  c.state = v;
}

void relay(const String &key, uint8_t pin) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = TYPE_RELAY;
  c.pin = pin;
  c.local = true;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void dimmer(const String &key, uint8_t pin) {
  int idx = findCalib(key);
  if (idx < 0) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  c.id = key;
  c.uid = GET_CHIP_ID() ^ ((uint32_t)key.c_str()[0] << 16);
  c.type = TYPE_DIMMER;
  c.pin = pin;
  c.local = true;
  pinMode(pin, OUTPUT);
  analogWrite(pin, 0);
}

float calibrate(const String &key, float raw) {
  int idx = findCalib(key);
  if (idx < 0) return raw;
  
  auto &c = calibrations[idx];
  return raw + c.correction;
}

Calibration* getCalib(const String &key) {
  int idx = findCalib(key);
  return (idx >= 0) ? &calibrations[idx] : nullptr;
}

RTCTime getTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  RTCTime rt = {
    (uint16_t)(timeinfo->tm_year + 1900),
    (uint8_t)(timeinfo->tm_mon + 1),
    (uint8_t)timeinfo->tm_mday,
    (uint8_t)timeinfo->tm_hour,
    (uint8_t)timeinfo->tm_min,
    (uint8_t)timeinfo->tm_sec
  };
  return rt;
}

uint16_t getMinutesOfDay() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  return timeinfo->tm_hour * 60 + timeinfo->tm_min;
}

uint32_t getUnixTime() {
  return (uint32_t)time(nullptr);
}

bool timeValid() {
  time_t now = time(nullptr);
  return now > 1704067200;
}

TimeSource getTimeSource() {
  return time_source;
}

void rtc(const RTCTime &t) {
  time_source = TIME_RTC;
}

void ntp(const RTCTime &t) {
  time_source = TIME_NTP;
}

// ========================================
// MESH CALLBACKS - Procesadas por sensors.cpp
// ========================================

void onRemoteSensorDiscovered(uint32_t remote_uid, const String &remote_ip, uint8_t sensor_id, uint8_t sensor_type, bool sensor_state, uint32_t sensor_value) {
  // Buscar o crear sensor remoto en calibrations[]
  int idx = -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!calibrations[i].local && 
        calibrations[i].device_uid == remote_uid &&
        calibrations[i].uid == sensor_id) {
      idx = i;
      break;
    }
  }
  
  // Si no existe, crear nuevo
  if (idx == -1) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        break;
      }
    }
  }
  
  if (idx >= 0) {
    auto &c = calibrations[idx];
    c.local = false;
    c.device_uid = remote_uid;
    c.device_ip = remote_ip;
    c.uid = sensor_id;
    c.type = (SensorType)sensor_type;
    c.state = sensor_state;
    c.last_update = millis();
    
    // Decodificar value
    if (c.type == SENSOR_LUMI) {
      c.value = (uint32_t)sensor_value;
    } else {
      float normalized = (float)sensor_value / 0xFFFFFFFF;
      c.value = normalized * (150.0 - (-50.0)) + (-50.0);
    }
    
    // Nombre automático si no existe
    if (c.id == "") {
      c.id = "Remote_" + String(remote_uid, HEX) + "_" + String(sensor_id);
    }
  }
}

void onRemoteCommand(uint8_t command_type, uint32_t sensor_id, uint32_t value, bool state) {
  // Procesar comando recibido (relay o dimmer)
  int idx = findCalibByUid(sensor_id);
  if (idx < 0) return;
  
  auto &c = calibrations[idx];
  
  switch (command_type) {
    case TYPE_RELAY:
      setRelay(c.id, state);
      break;
    case TYPE_DIMMER:
      handleDimmer(c.id, (int)value);
      break;
  }
  
  // Llamar custom hook si existe
  ::onCommandHook(sensor_id, command_type, value, state);
}

void handleRemoteActuator(uint32_t remote_uid, const String &remote_ip, const String &actuator_name, bool action, int level) {
  // Hacer POST HTTP al device remoto
  if (!WiFi.isConnected()) return;
  
  WiFiClient client;
  if (!client.connect(remote_ip.c_str(), 80)) {
    return;
  }
  
  String url;
  if (level >= 0) {
    url = "/dimmer?key=" + actuator_name + "&value=" + String(level);
  } else {
    url = "/toggle?key=" + actuator_name;
  }
  
  String request = "POST " + url + " HTTP/1.1\r\n";
  request += "Host: " + remote_ip + "\r\n";
  request += "Connection: close\r\n\r\n";
  
  client.print(request);
  client.stop();
}

}  // namespace sensors
