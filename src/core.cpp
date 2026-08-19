#include <ArduinoOTA.h>
#if defined(ESP32)
#include <esp_netif.h>
#endif
#include "core.h"
#include "config.h"
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"
#include "storage.h"
#include "log.h"

namespace core {

// Flags to ensure network-dependent services are initialized exactly once,
// only after the WiFi stack (STA or AP) is operational.

static bool ota_initialized = false;
static bool web_initialized = false;
static bool mesh_initialized = false;
static bool ota_enabled = false;
// Vars globales compartidas (no estáticas: accesibles para configuración).

static String uid;
String ssid;
String password;
static bool wifi_connected = false;
static bool wifi_connecting = false;
static unsigned long wifi_connect_start = 0;
static unsigned long last_attempt = 0;
static unsigned long last_report = 0;
static bool first_report = true;
GeneralSettings genset;

 // ================= HELPERS ===================

 /// Reanuda el servidor hotspot en modo AP tras una conexión WiFi fallida.
static void startOtaService();
static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  wifi_connecting = false;
  if (!web_initialized) {
    web::init();
    web_initialized = true;
  }
  web::server.close();
  delay(50);
  web::server.begin();
  if (!mesh_initialized) {
    mesh::init();
    mesh_initialized = true;
  }
  startOtaService();
  logger::coref("AP mode '%s' started", AP_SSID);
}

/// Single controlled OTA initialization path.
/// Only starts ArduinoOTA when the runtime flag says it is enabled and the
/// network interface is already operational. Never called from storage.
static void startOtaService() {
  if (ota_initialized || !ota_enabled) return;
  static String ota_hostname;
  ota_hostname = "qymera-" + String(GET_CHIP_ID());
  ArduinoOTA.setHostname(ota_hostname.c_str());
  ArduinoOTA.onStart([]() {
    logger::core("OTA: transfer started");
  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    if (p % 25 == 0) logger::coref("OTA: %u%%", p);
  });
  ArduinoOTA.onEnd([]() {
    logger::core("OTA: transfer complete");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    logger::warnf("OTA error: %d", (int)e);
  });
  ArduinoOTA.begin();
  ota_initialized = true;
  logger::core("ArduinoOTA started");
}

/// kicks off a non-blocking WiFi connection attempt.
/// WiFi status is polled from loop() via checkWiFiStatus().

static void startWiFi() {
  // Initialize ESP32 network interface layer (required before WiFi/EWiFi on ESP32)
  // Fixes: xQueueSemaphoreTake assert (queue.c:1709) on ESP32 boot
#if defined(ESP32)
  esp_netif_init();
  esp_event_loop_create_default();
#endif

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
    startOtaService();
    if (!mesh_initialized) {
      mesh::init();
      mesh_initialized = true;
    }
    if (!web_initialized) {
      web::init();
      web_initialized = true;
    }
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

  // Phase 1: Local subsystem initialization (no WiFi dependency)
  ota_enabled = storage::loadOtaFlag() == 1;
  if (ota_enabled) {
    if (storage::verifyOtaIntegrity()) {
      logger::core("OTA integrity verified");
    } else {
      logger::warnf("OTA integrity FAILED - disabling OTA");
      ota_enabled = false;
      storage::saveOtaFlag(0);
    }
  }

  // Correct boot order:
  // 1. init sensor subsystem
  // 2. register/discover local sensors and actuators (user initSatellite)
  // 3. load persistent configuration (UID-matched to registered devices)
  // 4. apply persisted relay states (writes GPIO exactly once, before any report)
  // 5. automations rules
  sensors::init();
  ::initSatellite();
  storage::loadCalibration();
  sensors::applyPersistedStates();
  automations::init();

  // Phase 2: Network startup
  // Network-dependent services (mesh, web, OTA) are initialized only when
  // WiFi is connected (in checkWiFiStatus()) or in AP mode (in startAP())
  // This prevents xQueueSemaphoreTake assert on ESP32 boot
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
  storage::saveOtaFlag(enabled ? 1 : 0);
  if (enabled) {
    if (!storage::verifyOtaIntegrity()) {
      ota_enabled = false;
      storage::saveOtaFlag(0);
      logger::warnf("OTA integrity FAILED - keeping OTA disabled");
      return;
    }
    // Network must be operational before ArduinoOTA.begin(). If it already is
    // (e.g. toggled via web), start now; otherwise the deferred network-init
    // path (startOtaService) will start it once WiFi is up.
    if (wifi_connected || WiFi.getMode() == WIFI_AP) {
      startOtaService();
    }
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
  /// 1) Manejo del servidor web (solo cuando está inicializado).
  if (web_initialized) {
    web::server.handleClient();
  }

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

  /// 4) Primera iteración: reporte inicial.
  //     Los estados persistentes ya fueron aplicados en begin() ANTES de
  //     cualquier reporte, así que aquí solo se publica el primer estado.
  if (first_report) {
    ::report();
    first_report = false;
    last_report = millis();
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

  if (ota_enabled && ota_initialized) {
    ArduinoOTA.handle();
  }
}

}  // namespace core
