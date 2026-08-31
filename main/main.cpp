/*
  Qymera on native ESP-IDF - example application (qymera-IDF branch)

  The user sketch registers integration callbacks via the Qymera API:
    - Qymera::setInit()    : initialize hardware (GPIO/LEDC config, bus drivers...)
    - Qymera::setReport()  : read hardware and report values via Qymera::xxx()
    - Qymera::setCommand() : optional hook for received commands

  The library handles: WiFi (STA/AP), HTTP web UI, UDP mesh protocol v5,
  automations, calibration and NVS-backed persistence. Everything runs on
  native ESP-IDF drivers - no Arduino, no external libraries (Matter bridge is
  opt-in).
*/

#include "Qymera.h"
#include "matter_bridge.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ================================
// Hardware integration callbacks
// ================================

static void myInit() {
  // init hardware (GPIO/LEDC/I2C/SPI...)
}

static void myReport() {
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

  Qymera::temperature("TEMP", tempF);
  Qymera::humidity("HUMI", humi);
  Qymera::luminosity("LUMI0", lumi);
  Qymera::airQ("AIRQ0", airQ);
  Qymera::pressure("PRES0", press);
  Qymera::level("LEVE0", level);
  Qymera::rain("RAIN0", rain);
  Qymera::contact("CONTACT", contact);
  Qymera::custom("GENERIC", generic);
  Qymera::relay("RELAY0", 5, true);
  Qymera::dimmer("DIMM0", 2, false);
}

static void myCommand(uint32_t /*uid*/, uint8_t /*type*/, int /*value*/, bool /*state*/) {
  // optional custom logic for received commands
}

// ================================
// Qymera main-loop task (single deterministic loop)
// ================================
static void qymera_loop_task(void * /*arg*/) {
  while (true) {
#if CONFIG_QYMERA_MATTER_ENABLE
    matter_bridge_loop();
#endif
    Qymera::loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ================================
// ESP-IDF entry point
// ================================
extern "C" void app_main(void) {
  Qymera::setInit(myInit);
  Qymera::setReport(myReport);
  Qymera::setCommand(myCommand);

  Qymera::begin();

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
