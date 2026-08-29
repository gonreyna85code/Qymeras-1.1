/*
  Qymera on native ESP-IDF - example application (qymera-IDF branch)

  The user sketch only needs to implement:
  1. initSatellite()     - initialize hardware (GPIO/LEDC config, bus drivers...)
  2. report()            - read hardware and report values via sensors::xxx()
  3. onCommandHook()     - custom logic for received commands

  The library handles: WiFi (STA/AP), HTTP web UI, UDP mesh protocol v5,
  automations, calibration and NVS-backed persistence. Everything runs on
  native ESP-IDF drivers - no Arduino, no external libraries (Matter bridge is
  opt-in).
*/

#include "Qymera.h"
#include "matter_bridge.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// ================================
// initSatellite - inicialización de hardware
// ================================
void initSatellite() {
  // init hardware (GPIO/LEDC/I2C/SPI...)
}

// ================================
// report - leer hardware y reportar valores
// ================================
void report() {
  // --- Valores de ejemplo/demostración ---
  constexpr float tempF    = 35.2f;
  constexpr float humi     = 35;
  constexpr uint16_t lumi  = 15535;
  constexpr uint8_t airQ   = 2;
  constexpr uint8_t press  = 101;
  constexpr uint8_t level  = 58;
  constexpr bool rain      = true;
  constexpr bool contact   = false;
  constexpr float generic  = 105.35f;

  sensors::temperature("TEMP", tempF);
  sensors::humidity("HUMI", humi);
  sensors::luminosity("LUMI0", lumi);
  sensors::airQ("AIRQ0", airQ);
  sensors::pressure("PRES0", press);
  sensors::level("LEVE0", level);
  sensors::rain("RAIN0", rain);
  sensors::contact("CONTACT", contact);
  sensors::custom("GENERIC", generic);
  sensors::relay("RELAY0", 5, true);
  sensors::dimmer("DIMM0", 2, false);
}

// ================================
// onCommandHook - lógica personalizada de comandos recibidos
// ================================
void onCommandHook(uint32_t /*uid*/, uint8_t /*type*/, int /*value*/, bool /*state*/) {
  // custom relay/dimmer logic for received commands
}

// ================================
// Qymera main-loop task (single deterministic loop)
// ================================
static void qymera_loop_task(void * /*arg*/) {
  while (true) {
#if CONFIG_QYMERA_MATTER_ENABLE
    matter_bridge_loop();
#endif
    core::loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ================================
// ESP-IDF entry point
// ================================
extern "C" void app_main(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  core::begin();

#if CONFIG_QYMERA_MATTER_ENABLE
  matter_bridge_init();
#endif

  xTaskCreatePinnedToCore(
    qymera_loop_task,
    "qymera_loop",
    CONFIG_QYMERA_LOOP_STACK_SIZE,
    nullptr,
    5,
    nullptr,
    tskNO_AFFINITY);
}