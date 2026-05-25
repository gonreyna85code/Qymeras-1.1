#include "web.h"
#include "core.h"
#include "config.h"
#include "sensors.h"

namespace sensors {

Calibration calibrations[MAX_SENSORS];

Fade fadeActuator = { 0, 0, 0, 0, 0, false };
Fade activeFades[MAX_SENSORS];
static RTCTime currentTime = {0, 0, 0, 0, 0, 0};
static RTCTime currentTimeRTC = {0, 0, 0, 0, 0, 0};
static RTCTime currentTimeNTP = {0, 0, 0, 0, 0, 0};
static bool rtcInitialized = false;
static bool ntpInitialized = false;

static bool isValidTime(const RTCTime &time) {
  return time.year >= 2020 &&
         time.month >= 1 && time.month <= 12 &&
         time.day >= 1 && time.day <= 31 &&
         time.hour <= 23 &&
         time.minute <= 59 &&
         time.second <= 59;
}

static void refreshCurrentTime() {
  if (rtcInitialized && isValidTime(currentTimeRTC)) {
    currentTime = currentTimeRTC;
    return;
  }
  if (ntpInitialized && isValidTime(currentTimeNTP)) {
    currentTime = currentTimeNTP;
    return;
  }
  currentTime = {0, 0, 0, 0, 0, 0};
}


// ----------- UTILIDADES -----------

void rtc(const RTCTime &time) {
  currentTimeRTC = time;
  rtcInitialized = isValidTime(time);
  refreshCurrentTime();
}

void ntp(const RTCTime &time) {
  currentTimeNTP = time;
  ntpInitialized = isValidTime(time);
  refreshCurrentTime();
}

RTCTime getTime() {
  return currentTime;
}

bool timeValid() {
  return isValidTime(currentTime);
}

TimeSource getTimeSource() {
  if (rtcInitialized && isValidTime(currentTimeRTC)) return TIME_RTC;
  if (ntpInitialized && isValidTime(currentTimeNTP)) return TIME_NTP;
  return TIME_NONE;
}

uint16_t getMinutesOfDay() {
  RTCTime now = getTime();
  return now.hour * 60 + now.minute;
}

uint32_t getUnixTime() {
  RTCTime now = getTime();
  if (!isValidTime(now)) return 0;
  uint32_t days = (now.year - 1970) * 365 + now.month * 30 + now.day;
  return days * 86400UL + now.hour * 3600UL + now.minute * 60UL + now.second;
}

uint32_t hash32(const String &s) {
  uint32_t h = 5381;
  for (size_t i = 0; i < s.length(); i++)
    h = ((h << 5) + h) + s[i];
  return h;
}

int findCalibByUid(uint32_t uid) {
  for (int i = 0; i < MAX_SENSORS; i++)
    if (calibrations[i].uid == uid)
      return i;
  return -1;
}

int findCalib(const String &key) {
  return findCalibByUid(hash32(key));
}

Calibration *getCalib(const String &key) {
  int i = findCalib(key);
  return i >= 0 ? &calibrations[i] : nullptr;
}

void registerCalib(const String &key) {
  uint32_t uid = hash32(key);
  if (findCalibByUid(uid) >= 0) return;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == 0) {
      auto &c = calibrations[i];
      c.id = key;
      c.uid = uid;
      c.pin = 255;
      c.type = SENSOR_NONE;
      c.state = false;
      c.value = 0;           
      c.min = 0;            
      c.max = 0;             
      c.correction = 0;      
      c.pulse = false;
      c.fade = 0;
      c.pulse_ms = 0;
      break;
    }
  }
}

// ----------- FADES -----------

void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activeFades[i].active) {
      activeFades[i] = { pin, from, to, millis(), dur, true };
      break;
    }
  }
}

void updateFades() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    Fade &f = activeFades[i];
    if (!f.active) continue;
    unsigned long elapsed = now - f.startTime;
    if (elapsed >= f.duration) {
      analogWrite(f.pin, f.endVal);
      f.active = false;
      continue;
    }
    int val = f.startVal + (float)elapsed / f.duration * (f.endVal - f.startVal);
    analogWrite(f.pin, val);
  }
}

void applyPersistedStates() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = calibrations[i];
    if (c.type == TYPE_RELAY && c.persist && c.pers_state) {
      digitalWrite(c.pin, c.pers_state ? LOW : HIGH);
      c.value = c.pers_state;
      core::setReport(i, c.uid, c.pers_state ? 1.0f : 0.0f, c.pers_state ? 1.0f : 0.0f, c.pers_state);
    }
  }
}

void applyFades() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = calibrations[i];
    if (c.type != TYPE_RELAY) continue;
    if (!c.pulse) continue;
    if (c.fade > 0 && millis() - c.fade >= c.pulse_ms) {
      digitalWrite(c.pin, HIGH);
      c.fade = 0;
      c.state = false;
      core::setReport(i, c.uid, 0, 0, false);
    }
  }
}

// ----------- CALIBRACIÓN -----------

float calibrate(const String &key, float raw) {
  Calibration *c = getCalib(key);
  if (!c) return raw;
  float v = raw + c->correction;
  if (c->type == SENSOR_LUMI) return v;
  if (c->type == SENSOR_TEMP) return v;
  if (c->max <= c->min) return v;
  if (c->type == SENSOR_HUMI) {
    float span = c->max - c->min;
    if (span <= 0.001f) return v;
    return constrain((v - c->min) / span * 100.0f, 0.0f, 100.0f);
  }
  return constrain(v, c->min, c->max);
}

// ----------- ACTUADORES -----------

void setRelay(const String &key, bool target) {
  Calibration *c = getCalib(key);
  if (!c || c->pin == 255) return;
  if (c->type != TYPE_RELAY) return;
  if (c->state == target) return;
  handleToggle(key);
}

void handleDimmer(const String &key, int value) {
  Calibration *c = getCalib(key);
  if (!c || c->pin == 255) return;
  int idx = findCalib(key);
  if (idx < 0) return;
  value = constrain(value, 0, 100);
  c->value = value;
  int pwmTarget = map(value, 0, 100, 0, 1023);
  if (value > 0) {
    c->state = true;
    startFade(key, c->pin, 0, pwmTarget, c->fade);
  } else {
    c->state = false;
    startFade(key, c->pin, pwmTarget, 0, c->fade);
  }
  float v = c->state ? c->value : 0;
  core::setReport(idx, c->uid, v, v, c->state);
}

void handleToggle(const String &key) {
  Calibration *c = getCalib(key);
  if (!c || c->pin == 255) return;
  int idx = findCalib(key);
  if (idx < 0) return;
  if (c->type == TYPE_DIMMER) {
    c->state = !c->state;
    int pwm = map(constrain(c->value, 0, 100), 0, 100, 0, 1023);
    if (c->state)
      startFade(key, c->pin, 0, pwm, c->fade);
    else
      startFade(key, c->pin, pwm, 0, c->fade);
    float v = c->state ? c->value : 0;
    core::setReport(idx, c->uid, v, v, c->state);
    return;
  }
  if (c->type == TYPE_RELAY) {
    if (c->pulse) {
      digitalWrite(c->pin, LOW);
      c->fade = millis();
      c->state = true;
      core::setReport(idx, c->uid, 1.0f, 1.0f, true);
    } else {
      bool next = !(digitalRead(c->pin) == LOW);
      digitalWrite(c->pin, next ? LOW : HIGH);
      c->state = next;
      core::setReport(idx, c->uid, next ? 1.0f : 0.0f, next ? 1.0f : 0.0f, next);
      if (c->persist) {
        c->pers_state = next;
        web::saveCalibration();
      }
    }
  }
}

// ----------- SENSORES -----------

void relay(const String &key, uint8_t pin) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  if (calibrations[idx].pin == 255) {
    calibrations[idx].pin = pin;
    calibrations[idx].type = TYPE_RELAY;
    pinMode(pin, OUTPUT);
  }
  bool st = (digitalRead(pin) == LOW);
  calibrations[idx].state = st;
  core::setReport(idx, calibrations[idx].uid, st ? 1.0f : 0.0f, st ? 1.0f : 0.0f, st);
}

void dimmer(const String &key, uint8_t pin) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  if (calibrations[idx].pin == 255) {
    calibrations[idx].pin = pin;
    calibrations[idx].type = TYPE_DIMMER;
    pinMode(pin, OUTPUT);
  }
  auto &c = calibrations[idx];
  float v = c.state ? c.value : 0;
  calibrations[idx].value = c.value;
  core::setReport(idx, c.uid, v, v, c.state);
}

void temperature(const String &key, float raw) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_TEMP;
  float calibrated = calibrate(key, raw);
  calibrations[idx].value = calibrated;
  core::setReport(idx, calibrations[idx].uid, calibrated, raw, false);
}

void humidity(const String &key, int raw) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_HUMI;
  float calibrated = calibrate(key, raw);
  calibrations[idx].value = calibrated;
  core::setReport(idx, calibrations[idx].uid, calibrated, raw, false);
}

void luminosity(const String &key, int raw) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_LUMI;
  float calibrated = calibrate(key, raw);
  calibrations[idx].value = calibrated;
  core::setReport(idx, calibrations[idx].uid, calibrated, raw, false);
}

void level(const String &key, int raw) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_LEVEL;
  float calibrated = calibrate(key, raw);
  calibrations[idx].value = calibrated;
  core::setReport(idx, calibrations[idx].uid, calibrated, raw, false);
}

void pressure(const String &key, float raw) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_PRESS;
  float calibrated = calibrate(key, raw);
  calibrations[idx].value = calibrated;
  core::setReport(idx, calibrations[idx].uid, calibrated, raw, false);
}

void rain(const String &key, bool v) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_RAIN;
  calibrations[idx].state = v;
  calibrations[idx].value = v;
  core::setReport(idx, calibrations[idx].uid, v ? 1.0f : 0.0f, v ? 1.0f : 0.0f, v);
}

void airQ(const String &key, const int &v) {
  registerCalib(key);
  int idx = findCalib(key);
  if (idx < 0) return;
  calibrations[idx].type = SENSOR_AIRQ;
  calibrations[idx].value = v;
  core::setReport(idx, calibrations[idx].uid, v, v, false);
}

}  // namespace sensors
