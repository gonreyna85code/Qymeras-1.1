#include <WiFiClient.h>
#include <time.h>
#include "config.h"
#include "sensors.h"
#include "core.h"
#include "mesh.h"
#include "web.h"


namespace sensors {

Calibration calibrations[MAX_SENSORS];
Fade activeFades[MAX_SENSORS];

static time_t time_offset = 0;
static TimeSource time_source = TIME_NONE;

static uint32_t makeSensorUid(uint8_t index) {
  return GET_CHIP_ID() + (uint32_t)index + 1;
}

static int findFreeCalib() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == 0) return i;
  }
  return -1;
}

static void bindLocalSensor(uint8_t idx, const String &name, SensorType type) {
  auto &c = calibrations[idx];
  c.id = idx;
  c.name = name;
  c.uid = makeSensorUid(idx);
  c.type = type;
  c.local = true;
  c.device_uid = GET_CHIP_ID();
  c.device_ip = WiFi.localIP().toString();
}

void init() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    calibrations[i] = Calibration();
    activeFades[i] = Fade();
  }
  mesh::setSensorDiscoveryCallback(onRemoteSensorDiscovered);
  mesh::setCommandCallback(onRemoteCommand);
}

void applyPersistedStates() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = calibrations[i];
    if (c.type != TYPE_RELAY) continue;
    Serial.printf(
      "Relay=%s pers=%d state=%d inv=%d pin=%d\n",
      c.name.c_str(),
      c.pers_state,
      c.state,
      c.inverted,
      c.pin);
    if (!c.persist) continue;
    if (!c.local) continue;
    pinMode(c.pin, OUTPUT);
    digitalWrite(c.pin, c.inverted ? !c.pers_state : c.pers_state);
    c.state = c.pers_state;
    mesh::setReport(i, c.uid, c.value, c.value, c.state);
  }
}

void applyFades() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activeFades[i].active) continue;
    auto &f = activeFades[i];
    auto &c = calibrations[i];
    if (!c.local) continue;
    unsigned long elapsed = millis() - f.startTime;
    if (elapsed >= f.duration) {
      int pwm = f.endVal;
      if (c.inverted)
        pwm = 255 - pwm;
      analogWrite(f.pin, pwm);
      f.active = false;
    } else {
      float progress = (float)elapsed / f.duration;
      int current =
        f.startVal + (f.endVal - f.startVal) * progress;
      int pwm = current;
      if (c.inverted)
        pwm = 255 - pwm;
      analogWrite(f.pin, pwm);
    }
  }
}

int findCalib(const String &key) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].name == key) return i;
  }
  return -1;
}

int findCalibByUid(uint32_t uid) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == uid) return i;
  }
  return -1;
}

int findCalibByIndex(uint8_t index) {
  if (index >= MAX_SENSORS) return -1;
  return calibrations[index].uid == 0 ? -1 : index;
}

void setRelay(const String &key, bool target) {
  int idx = findCalib(key);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type != TYPE_RELAY) return;

  if (!c.local) {
    // ---- Actuador REMOTO: enviar comando UDP al dispositivo propietario ----
    mesh::sendCommand(
      c.device_uid,
      c.device_ip,
      c.uid,
      (uint8_t)TYPE_RELAY,
      target ? 1u : 0u,
      target);
    return;
  }

  // ---- Actuador LOCAL: operar GPIO ----
  if (c.pulse && target) {
    digitalWrite(c.pin, (true  ^ c.inverted) ? HIGH : LOW);
    delay(c.pulse_ms);
    digitalWrite(c.pin, (false ^ c.inverted) ? HIGH : LOW);
    c.state = false;
  } else {
    digitalWrite(c.pin, (target ^ c.inverted) ? HIGH : LOW);
    c.state = target;
  }

  // ---- Persistencia en EEPROM ----
  if (c.persist) {
    c.pers_state = c.state;
    web::saveCalibrationSlot(idx);
  }

  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void handleDimmer(const String &key, int value) {
  int idx = findCalib(key);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type != TYPE_DIMMER) return;
  value = constrain(value, 0, 100);

  if (!c.local) {
    // ---- Actuador REMOTO: enviar comando UDP ----
    mesh::sendCommand(
      c.device_uid,
      c.device_ip,
      c.uid,
      (uint8_t)TYPE_DIMMER,
      (uint32_t)value,
      value > 0);
    return;
  }

  // ---- Actuador LOCAL: operar PWM ----
  int pwm_val = map(value, 0, 100, 0, 255);
  if (c.inverted)
    pwm_val = 255 - pwm_val;
  if (c.fade > 0) {
    int current = analogRead(c.pin);
    startFade(key, c.pin, current, pwm_val, c.fade);
  } else {
    analogWrite(c.pin, pwm_val);
  }
  c.value = value;
  c.state = (value > 0);
  mesh::setReport(idx, c.uid, c.value, c.state ? c.value : 0, c.state);
}

void handleToggle(uint32_t uid) {
  int idx = findCalibByUid(uid);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type == TYPE_RELAY) {
    setRelay(c.name, !c.state);
  } else if (c.type == TYPE_DIMMER) {
    c.state = !c.state;
    int pwm_val = map(c.value, 0, 100, 0, 255);
    if (c.inverted)
      pwm_val = 255 - pwm_val;
    if (c.fade > 0) {
      int current = analogRead(c.pin);
      startFade(
        c.name,
        c.pin,
        current,
        c.state ? pwm_val : 0,
        c.fade);
    } else {
      analogWrite(
        c.pin,
        c.state ? pwm_val : 0);
    }
    mesh::setReport(
      idx,
      c.uid,
      c.value,
      c.state ? c.value : 0,
      c.state);
  }
}

void handleDimmer(uint32_t uid, int value) {
  int idx = findCalibByUid(uid);
  if (idx < 0) return;
  handleDimmer(calibrations[idx].name, value);
}

void handleToggle(const String &key) {
  int idx = findCalib(key);
  if (idx < 0) return;
  handleToggle(calibrations[idx].uid);
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
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_TEMP);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void humidity(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_HUMI);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void luminosity(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_LUMI);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void level(const String &key, int raw) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_LEVEL);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void pressure(const String &key, float raw) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_PRESS);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void airQ(const String &key, const int &v) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_AIRQ);
  c.value = v;
  mesh::setReport(idx, c.uid, c.value, v, c.state);
}

void rain(const String &key, bool v) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_RAIN);
  c.state = v;
  c.value = v ? 1.0f : 0.0f;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void custom(const String &key, float raw) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_GENERIC);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void contact(const String &key, bool v) {
  int idx = findCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_CONTACT);
  c.state = v;
  c.value = v ? 0.0f : 1.0f;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void relay(const String &key, uint8_t pin, bool inverted) {
  int idx = findCalib(key);
  bool is_new = (idx < 0);
  if (is_new) {
    idx = findFreeCalib();
    if (idx < 0) return;
  }
  auto &c = calibrations[idx];
  if (is_new) {
    Serial.printf(
      "REGISTER idx=%d name=%s persist=%d pers=%d\n",
      idx,
      c.name.c_str(),
      c.persist,
      c.pers_state);
    bindLocalSensor(idx, key, TYPE_RELAY);
    c.pin = pin;
    c.inverted = inverted;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, inverted ? HIGH : LOW);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void dimmer(const String &key, uint8_t pin, bool inverted) {
  int idx = findCalib(key);
  bool is_new = (idx < 0);
  if (is_new) {
    idx = findFreeCalib();
    if (idx < 0) return;
  }
  auto &c = calibrations[idx];
  if (is_new) {
    bindLocalSensor(idx, key, TYPE_DIMMER);
    c.pin = pin;
    c.inverted = inverted;
    pinMode(pin, OUTPUT);
    int off_pwm = 0;
    if (c.inverted)
      off_pwm = 255;
    analogWrite(pin, off_pwm);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

static void updateTimeSensor() {
  time_t now = time(nullptr);
  if (now < 1704067200) return;
  int idx = findCalib("TIME");
  if (idx < 0) {
    idx = findFreeCalib();
    if (idx < 0) return;
    bindLocalSensor(idx, "TIME", SENSOR_TIME);
    calibrations[idx].state = true;
  }
  auto &c = calibrations[idx];
  c.value = (float)now;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

float calibrate(const String &key, float raw) {
  Calibration *c = getCalib(key);
  if (!c) return raw;
  float v = raw + c->correction;
  if (c->type == SENSOR_LUMI) return v;
  if (c->type == SENSOR_TEMP) return v;
  if (c->type == SENSOR_GENERIC) return v;
  if (c->type == SENSOR_PRESS) return v;
  if (c->max <= c->min) return v;
  if (c->type == SENSOR_HUMI || c->type == SENSOR_LEVEL) {
    float span = c->max - c->min;
    if (span <= 0.001f) return v;
    return constrain((v - c->min) / span * 100.0f, 0.0f, 100.0f);
  }
  return constrain(v, c->min, c->max);
}

Calibration *getCalib(const String &key) {
  int idx = findCalib(key);
  return (idx >= 0) ? &calibrations[idx] : nullptr;
}

RTCTime getTime() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
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
  struct tm *timeinfo = localtime(&now);
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
  updateTimeSensor();
}

void ntp(const RTCTime &t) {
  time_source = TIME_NTP;
  updateTimeSensor();
}

void initNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void updateNTPTime() {
  if (getTimeSource() == TIME_RTC)
    return;
  time_t now = time(nullptr);
  if (now < 1704067200)
    return;
  struct tm *timeinfo = localtime(&now);
  if (!timeinfo)
    return;
  RTCTime ntpTime = {
    static_cast<uint16_t>(timeinfo->tm_year + 1900),
    static_cast<uint8_t>(timeinfo->tm_mon + 1),
    static_cast<uint8_t>(timeinfo->tm_mday),
    static_cast<uint8_t>(timeinfo->tm_hour),
    static_cast<uint8_t>(timeinfo->tm_min),
    static_cast<uint8_t>(timeinfo->tm_sec)
  };
  ntp(ntpTime);
  updateTimeSensor();
}

// ========================================
// CALLBACK DE COMANDOS REMOTOS
// ========================================

/**
 * @brief Invocada por mesh::tick() cuando llega un paquete dirigido a este
 *        dispositivo (is_remote == false).  Busca el actuador por su uid y
 *        delega en setRelay / handleDimmer para ejecutar la acción LOCAL.
 *
 * @param command_type  Tipo de sensor/actuador del paquete (TYPE_RELAY, etc.).
 * @param sensor_id     UID del actuador destino.
 * @param value         Valor asociado (nivel dimmer o flag relay).
 * @param state         Estado ON/OFF.
 */
void onRemoteCommand(
  uint8_t command_type,
  uint32_t sensor_id,
  uint32_t value,
  bool state) {
  int idx = findCalibByUid(sensor_id);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (!c.local) return;  // sólo actuamos sobre sensores propios

  if (command_type == (uint8_t)TYPE_RELAY) {
    setRelay(c.name, state);
  } else if (command_type == (uint8_t)TYPE_DIMMER) {
    handleDimmer(c.name, (int)value);
  }
}

// ========================================
// MESH CALLBACKS - Procesadas por sensors.cpp
// ========================================

void onRemoteSensorDiscovered(
  uint32_t remote_uid,
  const String &remote_ip,
  uint32_t sensor_id,
  const String &sensor_name,
  uint8_t sensor_type,
  bool sensor_state,
  uint32_t sensor_value,
  float sensor_min,
  float sensor_max,
  float sensor_correction,
  uint8_t sensor_avail) {
  if (sensor_type == SENSOR_TIME) {
    int idx = findCalib("TIME");
    if (idx < 0) return;
    auto &c = calibrations[idx];
    if (c.correction == 0 && sensor_correction != 0) {
      c.correction = sensor_correction;
      web::saveCalibrationSlot(idx);
    }
    if (!timeValid() && sensor_value > 1704067200) {
      timeval tv;
      tv.tv_sec = (time_t)sensor_value;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
      rtc(getTime());
    }
    return;
  }
  int idx = -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!calibrations[i].local && calibrations[i].device_uid == remote_uid && calibrations[i].uid == sensor_id) {
      idx = i;
      break;
    }
  }
  bool is_new = false;
  if (idx == -1) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        is_new = true;
        break;
      }
    }
  }
  if (idx < 0) return;
  auto &c = calibrations[idx];
  c.local = false;
  c.device_uid = remote_uid;
  c.device_ip = remote_ip;
  c.id = idx;
  c.uid = sensor_id;
  c.type = (SensorType)sensor_type;
  if (is_new) c.avail = 0;
  c.state = sensor_state;
  c.last_update = millis();
  if (c.type == SENSOR_LUMI) {
    c.value = (uint32_t)sensor_value;
  } else {
    float normalized = (float)sensor_value / 0xFFFFFFFF;
    c.value = normalized * (150.0f - (-50.0f)) + (-50.0f);
  }
  if (sensor_name.length()) {
    c.name = sensor_name;
  } else if (c.name == "") {
    c.name = "Remote_" + String(remote_uid, HEX) + "_" + String(sensor_id);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

}  // namespace sensors
