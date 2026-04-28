#include "core.h"
#include <ArduinoOTA.h>
#include "config.h"
#include <vector>
#include <string>
#include "web.h"
#include "sensors.h"
#include "automations.h"

namespace core {

// ================= VARS ===================
static WiFiUDP udp;
ESP8266WebServer server(80);
static String uid;
String ssid, password;
static bool wifi_connected = false;
ReportEntry reports[MAX_SENSORS];
static unsigned long last_attempt = 0;
static unsigned long last_report = 0;
bool first_report = true;
GeneralSettings genset;
float MIN_VAL = -50.0f;
float MAX_VAL = 150.0f;

static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  server.close();
  delay(50);
  server.begin();
}

static void connectWiFi() {
  if (ssid == "") {
    startAP();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      udp.begin(genset.command_port);
      ArduinoOTA.begin();
      return;
    }
    delay(300);
  }
  startAP();
}

void begin() {
  uid = String(ESP.getChipId(), HEX);
  web::loadCredentials();
  web::loadCalibration();
  web::loadGeneralSettings();
  automations::init();
  connectWiFi();

  ////////////////API//////////////////

  server.on("/", web::handleRoot);
  server.on("/save", HTTP_POST, web::handleSave);
  server.on("/calib", web::handleCalib);
  server.on("/calib/set", HTTP_POST, web::handleCalibSet);
  server.on("/genset/save", HTTP_POST, web::handleGenSetSave);
  server.on("/rules", web::handleRules);
  server.on("/rules/set", HTTP_POST, web::handleSetRule);
  server.on("/rules/delete", HTTP_POST, web::handleDeleteRule);
  server.on("/factory", HTTP_POST, web::handleFactoryReset);
  server.on("/toggle", HTTP_POST, web::handleToggleApi);
  server.on("/dimmer", HTTP_POST, web::handleDimmerApi);
  server.begin();
  ::initSatellite();
}

bool is_connected() {
  return wifi_connected;
}

String get_uid() {
  return uid;
}

void setReport(uint8_t index, uint32_t uid, float value, float raw, bool state) {
  if (index >= MAX_SENSORS)
    return;
  reports[index].uid = uid;
  reports[index].value = value;
  reports[index].raw = raw;
  reports[index].state = state;
}

uint32_t encodeFloat(float v) {
  if (v < MIN_VAL)
    v = MIN_VAL;
  if (v > MAX_VAL)
    v = MAX_VAL;
  return (uint32_t)((v - MIN_VAL) / (MAX_VAL - MIN_VAL) * 0xFFFFFFFF);
}

void sendBinaryReport() {
  PacketHeader hdr;
  hdr.magic = 0xA5;
  hdr.version = 1;
  hdr.uid = ESP.getChipId();
  uint8_t count = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensors::calibrations[i].type != sensors::SENSOR_NONE && sensors::calibrations[i].avail)
      count++;
  }
  uint16_t payloadSize = count * sizeof(Packet);
  hdr.size = sizeof(PacketHeader) + payloadSize;
  udp.beginPacket("255.255.255.255", genset.broadcast_port);
  udp.write((uint8_t *)&hdr, sizeof(hdr));
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    if (c.type == sensors::SENSOR_NONE || !c.avail)
      continue;
    Packet pkt;
    pkt.id = c.uid;
    pkt.type = c.type;
    pkt.state = c.state ? 1 : 0;
    if (c.type == sensors::SENSOR_LUMI) {
      pkt.value = (uint32_t)c.value;
    } else {
      pkt.value = encodeFloat(c.value);
    }
    udp.write((uint8_t *)&pkt, sizeof(pkt));
  }
  udp.endPacket();
}

void loop() {
  server.handleClient();
  if (!wifi_connected && millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    connectWiFi();
  }
  if (!wifi_connected)
    return;
  ArduinoOTA.handle();

  if (first_report) {
    first_report = false;
    ::report();
    sensors::applyPersistedStates();
  }
  automations::tick(millis());
  sensors::applyFades();
  sensors::updateFades();
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    ::report();
    sendBinaryReport();
  }

  int packetSize = udp.parsePacket();
  if (packetSize < sizeof(PacketHeader))
    return;
  PacketHeader hdr;
  udp.read((uint8_t *)&hdr, sizeof(hdr));
  if (hdr.magic != 0xA5)
    return;
  if (hdr.version != 1)
    return;
  if (hdr.size != packetSize)
    return;
  int remaining = hdr.size - sizeof(PacketHeader);
  while (remaining >= sizeof(Packet)) {
    Packet pkt;
    udp.read((uint8_t *)&pkt, sizeof(pkt));
    remaining -= sizeof(Packet);
    int idx = sensors::findCalibByUid(pkt.id);
    if (idx < 0)
      continue;
    auto &c = sensors::calibrations[idx];
    ::onCommandHook(pkt.id, pkt.type, pkt.value, pkt.state);
    switch (pkt.type) {
      case sensors::TYPE_RELAY:
        sensors::setRelay(c.id, pkt.state);
        break;
      case sensors::TYPE_DIMMER:
        sensors::handleDimmer(c.id, pkt.value);
        break;
    }
  }
}

}  // namespace satellite
