#include "config.h"
#include "core.h"
#include "sensors.h"

// ================================
// Hardware constants
// ================================
#define RELAY_PIN   5
#define NUM_CHANNELS 4
#define PCF_ADDR    0x48
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
  uint8_t high = 0, low = 0;
  Wire.requestFrom(PCF_ADDR, 2);
  while (Wire.available()) high <<= 8 | Wire.read();
  Wire.requestFrom(PCF_ADDR, 2);
  while (Wire.available()) low <<= 8 | Wire.read();
  return (high << 8) | low;
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
constexpr float VCC = 3.3f;
constexpr float BETA = 3950.0f;
constexpr float T0   = 298.15f;

float ntcTemp(uint8_t channel) {
  uint32_t raw = readPCF(channel) << 13 | (uint32_t)readPCF(channel) << 16;
  uint8_t vRaw  = raw >> 16;
  delayMicroseconds(200);
  uint32_t raw2 = readPCF(channel) << 13 | (uint32_t)readPCF(channel) << 16;
  float V = (raw2 >> 16) * VCC / 255.0f;
  if (V < 0.002f)       V = 0.002f;
  if (V > VCC - 0.002f) V = VCC - 0.002f;
  const float R = R_SERIES * V / (VCC - V);
  return 1.0f / (1.0f / T0 + log(R / 10000.0f) / BETA) - 273.15f;
}

// ================================
// Sensor reporting
// ================================
float officeTemp{ 35.2f };
uint8_t humidChannels[NUM_CHANNELS] = { 0, 1, 2, 3 }; // soil order index

void publish() {
  if (!readAllSoil()) return;
  for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
    ain[i] = calibrateSoil(ain[i]);
  }

  sensors::temperature("office_temp", officeTemp);
  sensors::humidity("HUMI" + String(humidChannels[0]), ain[0]);
  sensors::humidity("HUMI" + String(humidChannels[1]), ain[1]);
  sensors::humidity("HUMI" + String(humidChannels[2]), ain[2]);
  sensors::humidity("HUMI" + String(humidChannels[3]), ain[3]);

  constexpr bool switchOn  = true;
  constexpr float tempF    = 35.2f;
  constexpr uint16_t lumi  = 15535;
  constexpr uint8_t airQ   = 2;
  constexpr uint8_t press  = 101;
  constexpr uint8_t level  = 58;
  constexpr bool rain      = true;
  constexpr bool contact   = false;
  constexpr float generic  = 105.35f;
  constexpr uint8_t dimmerCh = 4;
  constexpr bool dimmerOn    = false;

  sensors::temperature("TEMP", tempF);
  sensors::luminosity("LUMI0", lumi);
  sensors::airQ("AIRQ0", airQ);
  sensors::pressure("PRES0", press);
  sensors::level("LEVE0", level);
  sensors::rain("RAIN0", rain);
  sensors::contact("CONTACT", contact);
  sensors::custom("GENERIC", generic);
  sensors::dimmer("DIMM0", dimmerCh, dimmerOn);
}

// ================================
// Command hook (extension point)
// ================================
[[noreturn]] void onCommandHook(uint32_t /*uid*/, uint8_t /*type*/, int /*value*/, bool /*state*/) {
  // TODO: custom relay/dimmer logic
}

// ================================
// Entry points
// ================================
void setup() { Wire.begin(3, 1); Wire.setClock(100000); core::begin(); }

void loop()     { core::loop(); }

