#include "core.h"
#include <ArduinoOTA.h>
#include "config.h"
#include <vector>
#include <string>
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"
#include <time.h>

namespace core {

// ================= VARS ===================

static String uid;
String ssid, password;
static bool wifi_connected = false;
static unsigned long last_attempt = 0;
static unsigned long last_report = 0;
bool first_report = true;
GeneralSettings genset;

static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  web::server.close();
  delay(50);
  web::server.begin();
}

static void initNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

static void updateNTPTime() {
  if (sensors::getTimeSource() == sensors::TIME_RTC)
    return;
  time_t now = time(nullptr);
  if (now < 1704067200)  // 2024-01-01
    return;
  struct tm *timeinfo = localtime(&now);
  if (!timeinfo)
    return;
  sensors::RTCTime ntpTime = {
    static_cast<uint16_t>(timeinfo->tm_year + 1900),
    static_cast<uint8_t>(timeinfo->tm_mon + 1),
    static_cast<uint8_t>(timeinfo->tm_mday),
    static_cast<uint8_t>(timeinfo->tm_hour),
    static_cast<uint8_t>(timeinfo->tm_min),
    static_cast<uint8_t>(timeinfo->tm_sec)
  };
  sensors::ntp(ntpTime);
}

static void connectWiFi() {
  if (ssid == "") {
    startAP();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  SET_AUTO_CONNECT();
  SET_WIFI_SLEEP();
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      mesh::udp.begin(genset.command_port);
      initNTP();
      ArduinoOTA.begin();
      return;
    }
    delay(300);
  }
  startAP();
}

void begin() {
  uid = String(GET_CHIP_ID(), HEX);
  web::loadCredentials();
  web::loadGeneralSettings();
  sensors::init();
  web::loadCalibration();
  automations::init();
  connectWiFi();
  mesh::init();
  web::init();
  ::initSatellite();
}

bool is_connected() {
  return wifi_connected;
}

void loop() {

  if (!wifi_connected && millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    connectWiFi();
  }

  if (!wifi_connected)
    return;
  
  if (first_report) {
    first_report = false;
    ::report();
    sensors::applyPersistedStates();
  }

  web::server.handleClient();
  updateNTPTime(); 
  mesh::tick(millis());  
  automations::tick(millis());
  sensors::applyFades();
  sensors::updateFades();
  ArduinoOTA.handle();

  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    ::report();
    mesh::sendBinaryReport();
  }  
}

}  // namespace core
