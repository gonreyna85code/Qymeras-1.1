#include <ArduinoOTA.h>
#include "core.h"
#include "config.h"
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"
#include "log.h"

namespace core {

// ================= VARIABLES ===================
// Vars globales compartidas (no estáticas: accesibles para configuración).

static String uid;
String ssid;
String password;
static bool wifi_connected = false;
static unsigned long last_attempt = 0;
static unsigned long last_report = 0;
static bool first_report = true;
static bool ota_enabled = false;
GeneralSettings genset;

 // ================= HELPERS ===================

/// Reanuda el servidor hotspot en modo AP tras una conexión WiFi fallida.
static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  web::server.close();
  delay(50);
  web::server.begin();
  logger::coref("AP mode '%s' started", AP_SSID);
}

/// State-machine: kicks off a non-blocking WiFi connection attempt.
/// WiFi status is polled from loop() via checkWiFiStatus().
static bool wifi_connecting = false;
static unsigned long wifi_connect_start = 0;

static void startWiFi() {
  if (ssid == "") {
    startAP();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  SET_AUTO_CONNECT();
  SET_WIFI_SLEEP();
  wifi_connecting = true;
  wifi_connect_start = millis();
  logger::coref("WiFi connecting to %s", ssid.c_str());
}

/// Non-blocking WiFi status checker — call from loop().
/// Completes connection on success or falls back to AP on timeout.
static void checkWiFiStatus() {
  if (!wifi_connecting) return;

  if (WiFi.status() == WL_CONNECTED) {
    wifi_connecting = false;
    wifi_connected = true;
    sensors::initNTP();
    ArduinoOTA.begin();
    logger::coref("WiFi connected, IP:%s", WiFi.localIP().toString().c_str());
  } else if (millis() - wifi_connect_start >= 15000) {
    wifi_connecting = false;
    startAP();
    logger::warnf("WiFi timeout, starting AP '%s'", AP_SSID);
  }
}

// ================= INICIALIZACIÓN ===================

/// Inicializa la aplicación: serial, UID chip y dependencias, luego el stack WiFi
/// (con retento automático si no hay SSID guardado).
void begin() {
  Serial.begin(74880);
  delay(200);
  Serial.println();
  Serial.println("BOOT QYMERA");
  uid = String(GET_CHIP_ID(), HEX);
  logger::init();
  logger::core("Boot");
  web::loadCredentials();
  web::loadGeneralSettings();

  // Load OTA flag from EEPROM
  ota_enabled = EEPROM.read(EEPROM_OTA_FLAG_ADDR) == 1;
  if (ota_enabled) {
    ArduinoOTA.begin();
    logger::core("OTA enabled");
  }

  sensors::init();
  ::initSatellite();
  web::loadCalibration();
  automations::init();
  mesh::init();
  logger::coref("Mesh UDP ready (bc:%u, cmd:%u)", genset.broadcast_port, genset.command_port);
  web::init();
  startWiFi();
  logger::coref("Free heap: %u B", ESP.getFreeHeap());
}

// ================= ACCESORIOS ===================

/// Devuelve si el dispositivo ya está conectado a una red (no está en AP).
bool is_connected() {
  return wifi_connected;
}

// ================= OTA CONTROL =================

void setOtaEnabled(bool enabled) {
  ota_enabled = enabled;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_OTA_FLAG_ADDR, enabled ? 1 : 0);
  EEPROM.commit();
  if (enabled) {
    ArduinoOTA.begin();
    logger::core("OTA enabled");
  } else {
    logger::core("OTA disabled");
  }
}

bool isOtaEnabled() {
  return ota_enabled;
}

// ================= LOOP PRINCIPAL ===================

/// Bucle de la aplicación: maneja HTTP, reconexión Wi-Fi si cae,
/// reportes periódicos y tareas de sensores/mesh/automáticas.
void loop() {
  /// 1) Manejo del servidor web (siempre activo).
  web::server.handleClient();

  /// 2) Poll WiFi connection state (non-blocking).
  checkWiFiStatus();

  /// 3) Reconexion si perdimos WiFi y paso el timeout definido.
  if (!wifi_connected && !wifi_connecting &&
      millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    startWiFi();
  }

  /// Si no hay red, usar ESP-NOW; si hay red, usar UDP
  mesh::setTransport(wifi_connected ? mesh::TRANSPORT_UDP : mesh::TRANSPORT_ESPNOW);

  /// 4) Primera iteración: APLICAR estados persistentes ANTES del reporte
  //     para evitar piso — los relays deben reflejar su estado persistente
  //     antes de que report() publique valores al mesh.
  if (first_report) {
    sensors::applyPersistedStates();
    ::report();
    first_report = false;
    last_report = millis();
    Serial.println();
    Serial.println("apply");
  }

  /// 4) Tareas periódicas: clock NTP, mesh tick, automatización.
  sensors::updateNTPTime();
  mesh::tick(millis());
  automations::tick(millis());
  sensors::applyFades();
  sensors::checkPulses();

  /// 6) Reporte periódico cuando llegue el intervalo configurado.
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    ::report();
    mesh::sendBinaryReport();
  }

  if (ota_enabled) {
    ArduinoOTA.handle();
  }
}

}  // namespace core
