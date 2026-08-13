#include <ArduinoOTA.h>
#include "core.h"
#include "config.h"
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"
#include "storage.h"
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
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("BOOT QYMERA");
  uid = String(GET_CHIP_ID(), HEX);
  logger::init();
  logger::core("Boot");
  storage::loadCredentials(ssid, password);
  logger::core("Credentials loaded");
  storage::loadGeneralSettings(genset.broadcast_port, genset.command_port, genset.report_interval);
  logger::core("Settings loaded");

  // Verify firmware integrity if OTA is enabled
  bool ota_flag = storage::loadOtaFlag() == 1;
  if (ota_flag) {
    if (storage::verifyOtaIntegrity()) {
      logger::core("OTA integrity verified");
      ArduinoOTA.begin();
    } else {
      logger::core("OTA integrity FAILED - disabling OTA");
      storage::setOtaEnabled(false);
    }
  }
  
  if (!ota_flag) {
    ArduinoOTA.begin();  // Initialize even if disabled (but won't handle)
  }

  sensors::init();
  ::initSatellite();
  storage::loadCalibration();
  automations::init();
  startWiFi();
  mesh::init();
  logger::coref("Mesh UDP ready (bc:%u, cmd:%u)", genset.broadcast_port, genset.command_port);
  web::init();
  logger::coref("Free heap: %u B", ESP.getFreeHeap());
}

// ================= ACCESORIOS ===================

/// Devuelve si el dispositivo ya está conectado a una red (no está en AP).
bool is_connected() {
  return wifi_connected;
}

// ================= OTA CONTROL =================

void setOtaEnabled(bool enabled) {
  storage::setOtaEnabled(enabled);
}

bool isOtaEnabled() {
  return storage::isOtaEnabled();
}

// ================= OTA INTEGRITY ===============// OTA firmware integrity verification// Computes a hash over the firmware image and compares with stored expected value.// Provides integrity verification (detects corruption), not authenticity.// A mismatch means the firmware image has changed since last provisioning.static bool ota_integrity_verified = false;static uint32_t calculateFirmwareHash() {  // Compute a hash over the firmware image using available bytes.  // On platforms with limited flash access, we hash the first 256 bytes  // as a practical compromise between security and feasibility.  // This is NOT a full-image hash but is much better than the previous 64-byte check.  uint32_t hash = 0;  uint8_t *ptr = (uint8_t *)0x0000;  uint32_t sketch_size = ESP.getSketchSize();  int bytes_to_hash = (sketch_size < 256) ? sketch_size : 256;  for (int i = 0; i < bytes_to_hash; i++) {    hash = (hash << 5) | (hash >> 27);  // rotate left 5    hash += ptr[i];  }  return hash;}bool verifyOtaIntegrity() {  // Read expected hash from EEPROM  eeprom_begin();  uint32_t expected = eeprom_read(EEPROM_OTA_CHECKSUM_ADDR);  eeprom_commit();    if (expected == 0) {    // No hash stored yet - store current firmware hash and accept.    // This is the provisioning step: first run stores the baseline hash.    eeprom_begin();    eeprom_write(EEPROM_OTA_CHECKSUM_ADDR, calculateFirmwareHash());    eeprom_commit();    ota_integrity_verified = true;    logger::core("OTA baseline hash stored (provisioning step)");    return true;  }    uint32_t actual = calculateFirmwareHash();  ota_integrity_verified = (actual == expected);    if (!ota_integrity_verified) {    logger::warnf("OTA integrity FAILED (stored:%08X actual:%08X)", expected, actual);  } else {    logger::core("OTA integrity verified");  }  return ota_integrity_verified;}bool isOtaIntegrityVerified() {  return ota_integrity_verified;}// ================= LOOP PRINCIPAL ===================

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

  if (storage::isOtaEnabled()) {
    ArduinoOTA.handle();
  }
}

}  // namespace core
