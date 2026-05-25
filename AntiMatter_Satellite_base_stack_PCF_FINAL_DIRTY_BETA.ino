#include "config.h"
#include "core.h"
#include "sensors.h"
//#include <Wire.h>
#include <math.h>
#include <stdint.h>

#define Relay_pin 0
// #define N_CH 4
// #define PCF8591_ADDR 0x48
// uint8_t ain[N_CH];
// float SERIES_R = 10000.0;

void initSatellite() {
  // Wire.begin(3, 1);
  // Wire.setClock(100000);
  // pinMode(Relay_pin, OUTPUT);
  // digitalWrite(Relay_pin, HIGH);
  // pinMode(2, OUTPUT);
  // digitalWrite(2, HIGH);
}

// uint8_t readPCF(uint8_t channel) {
//   Wire.beginTransmission(PCF8591_ADDR);
//   Wire.write(0x40 | (channel & 0x03));
//   Wire.endTransmission();
//   Wire.requestFrom(PCF8591_ADDR, 2);
//   Wire.read();
//   Wire.read();
//   Wire.requestFrom(PCF8591_ADDR, 2);
//   Wire.read();
//   return Wire.read();
// }

// void probes_on() {
//   digitalWrite(2, LOW);
// }

// void probes_off() {
//   digitalWrite(2, HIGH);
// }

// // uint16_t read_lux(uint8_t channel) {
// //   const float VCC = 3.3f;
// //   const float SERIES_R = 10000.0f;
// //   uint32_t acc = 0;
// //   for (uint8_t i = 0; i < 32; i++) {
// //     readPCF(channel);
// //     delayMicroseconds(200);
// //     acc += readPCF(channel);
// //     delayMicroseconds(200);
// //   }
// //   float raw8 = acc / 32.0f;
// //   float inv = 255.0f - raw8;
// //   if (inv < 1.0f) inv = 1.0f;
// //   float V = (inv / 255.0f) * VCC;
// //   if (V < 0.002f) V = 0.002f;
// //   float R = SERIES_R * (VCC / V - 1.0f);
// //   const float CAL_ALPHA = 1.9e9f;
// //   const float CAL_BETA = -1.45f;
// //   float lux = CAL_ALPHA * powf(R, CAL_BETA);
// //   if (raw8 < 6) lux *= 8.0f;
// //   else if (raw8 < 12) lux *= 4.0f;
// //   else if (raw8 < 20) lux *= 2.0f;
// //   lux *= 10.0f;
// //   if (!isfinite(lux) || lux < 0.0f) lux = 0.0f;
// //   if (lux > 120000.0f) lux = 120000.0f;
// //   return (uint16_t)roundf(lux);
// // }

// uint8_t read_soil_all() {
//   uint16_t acc_on[N_CH] = { 0 };
//   probes_on();
//   delay(500);
//   for (uint8_t k = 0; k < 8; k++) {
//     for (uint8_t ch = 0; ch < N_CH; ch++) {
//       acc_on[ch] += readPCF(ch);
//       delayMicroseconds(120);
//     }
//   }
//   probes_off();
//   for (uint8_t ch = 0; ch < N_CH; ch++) {
//     uint8_t on = acc_on[ch] / 8;
//     uint8_t soil = map(on, 231, 136, 0, 100);
//     ain[ch] = constrain(soil, 0, 100);
//   }
//   return 1;
// }

// float read_temp(uint8_t channel) {
//   constexpr float VCC = 3.3f;
//   constexpr float BETA = 3950.0f;
//   constexpr float T0 = 298.15f;
//   constexpr float R0 = 10000.0f;
//   readPCF(channel);
//   delayMicroseconds(200);
//   float V = readPCF(channel) * 3.3f / 255.0f;
//   if (V < 0.002f) V = 0.002f;
//   if (V > VCC - 0.002f) V = VCC - 0.002f;
//   float R = SERIES_R * (V / (VCC - V));
//   float T = 1.0f / (1.0f / T0 + log(R / R0) / BETA) - 273.15f;
//   return T;
// }

void report() {
  // read_soil_all();
  // uint8_t soil3 = ain[3];
  // uint8_t soil1 = ain[1];
  // uint8_t soil2 = ain[2];
  // uint8_t soil0 = ain[0];
  // //uint16_t lux = read_lux(1);
  // delayMicroseconds(200);
  // float temp = read_temp(2);

  // sensors::temperature("office_temp", temp);  // float
  // sensors::humidity("HUMI0", soil0);          // 0-100
  // sensors::humidity("HUMI1", soil3);          // 0-100
  // sensors::humidity("HUMI2", soil1);          // 0-100
  // sensors::humidity("HUMI3", soil2);          // 0-100  
  // sensors::relay("Switch", Relay_pin);        // pin number
  sensors::luminosity("LUMI0", 15535);         // uint16_t, 0–65535 (normalized, Google Home expected range)
  sensors::airQ("AIRQ0", 2);                  // 0(GOOD)/1(WARN)/2(BAD)
  sensors::pressure("PRES0", 101);             // float
  sensors::level("LEVE0", 58);                // 0/100
  sensors::rain("RAIN0", true);               // true/false
  // sensors::dimmer("DIMM0", 2);                // pin number
}


void onCommandHook(uint32_t uid, uint8_t type, int value, bool state) {
  // Custom logic for relays/dimmers
}


void setup() {
  core::begin();
}

void loop() {
  core::loop();
}
