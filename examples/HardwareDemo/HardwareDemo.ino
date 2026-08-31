/*
  HardwareDemo - Example using the Qymera library
  =================================================
  
  This sketch demonstrates using a PCF8591 (8-bit) ADC to read:
  - Soil moisture (4 channels)
  - NTC temperature
  - Simulated sensors (temp, light, pressure, etc.)
  - Relay and dimmer actuators
  
The user sketch only needs to implement:
  1. Qymera::init()    - initialize hardware libraries (Wire, etc.)
  2. Qymera::report()  - read hardware and report values via Qymera::xxx()
  3. Qymera::onCommand() - custom logic for received commands

  The library handles: WiFi, web server, UDP mesh, automations, EEPROM.
*/

#include <Wire.h>
#include <Qymera.h>

// ================================
// Hardware constants
// ================================
#define RELAY_PIN    5
#define NUM_CHANNELS 4
#define PCF_ADDR     0x48
constexpr float R_SERIES = 10000.0f;

#define SOIL_ACCUM_LOOPS  8
#define SOIL_ACCUM_DELAY  120

// ================================
// ADC (PCF8591) driver
// ================================
uint8_t ain[NUM_CHANNELS]{};

uint8_t readPCF(uint8_t channel) {
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(0x40 | (channel & 3));
  Wire.endTransmission();
  Wire.requestFrom(PCF_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

void drainADC() { /* clear stale reads */ }

// ================================
// Soil probe driver
// ================================
static void probes_enable()   { digitalWrite(2, LOW); }
static void probes_disable()  { digitalWrite(2, HIGH); }

static inline uint8_t calibrateSoil(uint16_t acc_val) {
  return constrain(map(acc_val / SOIL_ACCUM_LOOPS, 231, 136, 0, 100), 0, 100);
}

uint8_t readAllSoil() {
  probes_enable();
  delay(500);
  for (uint8_t iter = 0; iter < SOIL_ACCUM_LOOPS; ++iter) {
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
      ain[ch] += readPCF(ch);
      delayMicroseconds(SOIL_ACCUM_DELAY);
    }
  }
  probes_disable();
  return 1; // success sentinel
}

// ================================
// NTC temperature driver
// ================================
constexpr float VCC      = 3.3f;
constexpr float BETA     = 3950.0f;
constexpr float TEMP_REF = 298.15f;  // 25°C en Kelvin

float ntcTemp(uint8_t channel) {
  // PCF8591 es un ADC de 8 bits (0-255). Leemos dos veces y promediamos
  // para reducir ruido, luego convertimos a voltaje.
  uint16_t raw1 = readPCF(channel);
  delayMicroseconds(200);
  uint16_t raw2 = readPCF(channel);
  uint16_t raw  = (raw1 + raw2) / 2;
  float V = (float)raw * VCC / 255.0f;
  if (V < 0.002f)       V = 0.002f;
  if (V > VCC - 0.002f) V = VCC - 0.0f;
  const float R = R_SERIES * V / (VCC - V);
  return 1.0f / (1.0f / TEMP_REF + log(R / 10000.0f) / BETA) - 273.15f;
}

// ================================
// Sensor state
// ================================
float officeTemp{35.2f};
uint8_t humidChannels[NUM_CHANNELS] = {0, 1, 2, 3}; // soil order index

// ================================
// Qymera::init - inicialización de hardware
// ================================
void Qymera::init() {
  Wire.begin(3, 1);
  Wire.setClock(100000);
}

// ================================
// Qymera::report - leer hardware y reportar valores
// ================================
void Qymera::report() {
  if (!readAllSoil()) return;
  for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
    ain[i] = calibrateSoil(ain[i]);
  }

  // --- Sensors locales (auto-registro en sensors.cpp) ---
  Qymera::temperature("office_temp", officeTemp);
  Qymera::humidity("HUMI" + String(humidChannels[0]), ain[0]);
  Qymera::humidity("HUMI" + String(humidChannels[1]), ain[1]);
  Qymera::humidity("HUMI" + String(humidChannels[2]), ain[2]);
  Qymera::humidity("HUMI" + String(humidChannels[3]), ain[3]);

  // --- Values de ejemplo/demonstración ---
  constexpr float tempF    = 35.2f;
  constexpr uint16_t lumi   = 15535;
  constexpr uint8_t airQ   = 2;
  constexpr uint8_t press  = 101;
  constexpr uint8_t level  = 58;
  constexpr bool rain       = true;
  constexpr bool contact    = false;
  constexpr float generic   = 105.35f;
  constexpr uint8_t dimmerCh = 4;
  constexpr bool dimmerOn    = false;

  Qymera::temperature("TEMP", tempF);
  Qymera::luminosity("LUMI0", lumi);
  Qymera::airQ("AIRQ0", airQ);
  Qymera::pressure("PRES0", press);
  Qymera::level("LEVE0", level);
  Qymera::rain("RAIN0", rain);
  Qymera::contact("CONTACT", contact);
  Qymera::custom("GENERIC", generic);
  Qymera::relay("RELAY0", RELAY_PIN, true);    // actuador local — persiste en EEPROM
  Qymera::dimmer("DIMM0", dimmerCh, dimmerOn);
}

// ================================
// Qymera::onCommand - lógica personalizada de comandos recibidos
// ================================
void Qymera::onCommand(uint32_t /*uid*/, uint8_t /*type*/, int /*value*/, bool /*state*/) {
  // TODO: custom relay/dimmer logic
}

// ================================
// Entry points - delega todo a la librería
// ================================
void setup()   { Qymera::begin(); }
void loop()    { Qymera::loop(); }
