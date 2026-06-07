#pragma once
#include <Arduino.h>
#include "config.h"

namespace sensors {

enum SensorType : uint8_t {
  SENSOR_NONE,
  SENSOR_LUMI,
  SENSOR_HUMI,
  SENSOR_TEMP,
  SENSOR_PRESS,
  SENSOR_LEVEL,
  SENSOR_AIRQ,
  SENSOR_RAIN,
  TYPE_DIMMER,
  TYPE_RELAY,
  SENSOR_TIME,
  SENSOR_GENERIC,
  SENSOR_CONTACT
};

struct Calibration {
  float min;
  float max;
  float correction;
  uint8_t avail;
  bool persist;
  bool pers_state;
  bool pulse;
  uint32_t pulse_ms;
  uint32_t fade;
  bool state;
  float value;
  SensorType type;
  uint8_t pin;
  String id;
  uint32_t uid = 0;
  bool local = true;              // true=local, false=remote mirror
  String device_ip = "";          // IP del device remoto si local=false
  uint32_t device_uid = 0;        // UID del device remoto
  unsigned long last_update = 0;  // Timestamp último update
};

struct Fade {
  uint8_t pin;
  int startVal;
  int endVal;
  unsigned long startTime;
  unsigned long duration;
  bool active;
};

struct RTCTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

enum TimeSource : uint8_t {
  TIME_NONE,
  TIME_NTP,
  TIME_RTC
};

extern Calibration calibrations[MAX_SENSORS];
extern Fade activeFades[MAX_SENSORS];
void init();
void applyPersistedStates();
void applyFades();
extern int findCalib(const String &key);
extern int findCalibByUid(uint32_t uid);
void setRelay(const String &key, bool target);
void handleDimmer(const String &key, int value);
void handleToggle(const String &key);
RTCTime getTime();
uint16_t getMinutesOfDay();
uint32_t getUnixTime();
bool timeValid();
TimeSource getTimeSource();

// Time
void rtc(const RTCTime &time);
void ntp(const RTCTime &time);

// Sensores
void temperature(const String &key, float raw);
void humidity(const String &key, int raw);
void luminosity(const String &key, int raw);
void level(const String &key, int raw);
void pressure(const String &key, float raw);
void airQ(const String &key, const int &v);
void rain(const String &key, bool v);
void generic(const String &key, float raw);
void contact(const String &key, bool v);

// Actuadores
void relay(const String &key, uint8_t pin);
void dimmer(const String &key, uint8_t pin);
void handleToggle(const String &key);
void handleDimmer(const String &key, int value);

// Fades
void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur);
void updateFades();

// Calibración
float calibrate(const String &key, float raw);
Calibration* getCalib(const String &key);

// Mesh callbacks - Procesadas por sensors.cpp
void onRemoteSensorDiscovered(uint32_t remote_uid, const String &remote_ip, uint32_t sensor_id, const String &sensor_name, uint8_t sensor_type, bool sensor_state, uint32_t sensor_value);
void onRemoteCommand(uint8_t command_type, uint32_t sensor_id, uint32_t value, bool state);
void handleRemoteActuator(uint32_t remote_uid, const String &remote_ip, const String &actuator_name, bool action, int level = 0);

}  // namespace sensors
