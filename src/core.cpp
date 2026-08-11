#include <ArduinoOTA.h>
#include "core.h"
#include "config.h"
#include "web.h"
#include "sensors.h"
#include "mesh.h"
#include "automations.h"

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
}

/// Configura y conecta a una red Wi-Fi con nombre/contraseña dados,
/// o reanuda AP si no hay credenciales guardadas.
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
    auto status = WiFi.status();
    if (status == WL_CONNECTED) {
      wifi_connected = true;
      sensors::initNTP();
      ArduinoOTA.begin();
      return;
    }
    delay(300);
  }
  startAP();
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
  web::loadCredentials();
  web::loadGeneralSettings();
  sensors::init();
  ::initSatellite();
  web::loadCalibration();
  automations::init();
  connectWiFi();
  mesh::init();
  web::init();
}

// ================= ACCESORIOS ===================

/// Devuelve si el dispositivo ya está conectado a una red (no está en AP).
bool is_connected() {
  return wifi_connected;
}

// ================= LOOP PRINCIPAL ===================

/// Bucle de la aplicación: maneja HTTP, reconexión Wi-Fi si cae,
/// reportes periódicos y tareas de sensores/mesh/automáticas.
void loop() {
  /// 1) Manejo del servidor web (siempre activo).
  web::server.handleClient();

  /// 2) Reconexión si perdimos WiFi y pasó el timeout definido.
  if (!wifi_connected && millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    connectWiFi();
  }

  /// Si no hay red, sale del loop principal (solo el HTTP sigue corriendo).
  if (!wifi_connected) {
    return;
  }

  /// 3) Primera iteración: APLICAR estados persistentes ANTES del reporte
  //     para evitar pise — los relays deben reflejar su estado persisted
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
  sensors::updateFades();

  /// 5) Reporte periódico cuando llegue el intervalo configurado.
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    ::report();
    mesh::sendBinaryReport();
  }

  ArduinoOTA.handle();
}

}  // namespace core
