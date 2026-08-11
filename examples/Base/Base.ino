/*
  Base - Minimal Qymera Example
  =============================

  The user sketch only needs to implement:
  1. initSatellite() - initialize hardware libraries (Wire, etc.)
  2. report()       - read hardware and report values via sensors::xxx()
  3. onCommandHook() - custom logic for received commands

  The library handles: WiFi, web server, UDP mesh, automations, EEPROM.
*/

#include <Qymera.h>



// ================================
// initSatellite - inicialización de hardware
// ================================
void initSatellite() {
  setSerialEnabled(false);
  // init hardware
}

// ================================
// report - leer hardware y reportar valores
// ================================
void report() { 

  // --- Values de ejemplo/demonstración ---
  constexpr float tempF    = 35.2f;
  constexpr float humi    = 35;
  constexpr uint16_t lumi   = 15535;
  constexpr uint8_t airQ   = 2;
  constexpr uint8_t press  = 101;
  constexpr uint8_t level  = 58;
  constexpr bool rain       = true;
  constexpr bool contact    = false;
  constexpr float generic   = 105.35f;

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
  // custom relay/dimmer logic for recieved commands
}

// ================================
// Entry points - delega todo a la librería
// ================================
void setup()   { core::begin(); }
void loop()    { core::loop(); }
