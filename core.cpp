#include <ArduinoOTA.h>
#include <vector>
#include <string>
#include "core.h"
#include "config.h"
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"

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
      sensors::initNTP();
      ArduinoOTA.begin();
      return;
    }
    delay(300);
  }
  startAP();
}

void begin() {
  Serial.begin(74880);
  delay(200);
  Serial.println();
  Serial.println("BOOT QYMERA");
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
  web::server.handleClient();
  if (!wifi_connected && millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    connectWiFi();
  }
  if (!wifi_connected)
    return;

  if (first_report) {
    ::report();
    first_report = false;
    sensors::applyPersistedStates();
    Serial.println();
    Serial.println("apply");
  }
  sensors::updateNTPTime();
  mesh::tick(millis());
  automations::tick(millis());
  sensors::applyFades();
  sensors::updateFades();
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    ::report();
    mesh::sendBinaryReport();
  }
  ArduinoOTA.handle();
}

}  // namespace core
