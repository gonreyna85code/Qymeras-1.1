#pragma once
/*
  config.h - Qymera configuration for the native ESP-IDF port (qymera-IDF)

  This branch builds Qymera solely with ESP-IDF drivers. There is deliberate
  #error on the Arduino framework: the historical Arduino build lives on the
  `main` branch. All platform access goes through src/hal/ (qhal/qudp/
  qhttpserver), so the deterministic core modules stay portable.
*/

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "esp_system.h"
#include "esp_attr.h"

/* =========================
   PLATAFORMA
   ========================= */

#if defined(ESP_PLATFORM) && !defined(ARDUINO)
#define PLATFORM_ESP_IDF 1

#include "hal/qstr.h"
#include "hal/qhal.h"
#include "hal/qudp.h"
#include "hal/qhttpserver.h"

typedef QymeraServer WebServerCompat;

#define PROGMEM
#define PSTR(s) s
#define PROGMEM_ATTR
#define ICACHE_FLASH_ATTR
#define ICACHE_FLASH

#define GET_CHIP_ID() qhal_chip_id()
#define SET_WIFI_SLEEP() ((void)0)
#define SET_AUTO_CONNECT() ((void)0)
#define RESET_MCU() qhal_restart()

#else
#error "qymera-IDF branch requires a native ESP-IDF build (ESP_PLATFORM && !ARDUINO). Arduino builds live on the 'main' branch."
#endif

/* =========================
   ARDUINO-COMPAT MINI ALIASES
   Only the tokens the Qymera modules actually use. Everything maps to native
   ESP-IDF via src/hal/. Kept intentionally tiny.
   ========================= */

#define LOW 0
#define HIGH 1
#define OUTPUT 1
#define INPUT 0

inline void pinMode(uint8_t pin, uint8_t mode) {
  if (mode == OUTPUT) qhal_pin_output(pin);
  else                qhal_pin_input(pin);
}
inline void digitalWrite(uint8_t pin, uint8_t val) {
  qhal_digital_write(pin, val != LOW);
}
inline uint8_t digitalRead(uint8_t pin) {
  return qhal_digital_read(pin) ? HIGH : LOW;
}
inline void delay(unsigned long ms) {
  qhal_delay(ms);
}

template <typename T> inline T constrain(T v, T lo, T hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/* =========================
   LIMITES DEL SISTEMA
   ========================= */

#define MAX_SENSORS 64
#define MAX_PERSISTED_SENSORS 40
#define MAX_RULES 20

/* =========================
   PERSISTENCIA (imagen EEPROM 4 KiB)
   Offsets historicales; la imagen completa vive en NVS como un único blob y
   todos los accesos byte-a-byte ocurren sobre una copia en RAM.
   ========================= */

#define EEPROM_SIZE 4096

/* Relay state (LEGACY/RESERVED region)
   Relay persistence now lives inside CalibrationPersist slots; this region is
   kept only as the layout anchor and is zeroed by factory reset. Do not use. */
#define EEPROM_RELAY_STATE_START 0
#define EEPROM_RELAY_STATE_SIZE 10

/* WiFi credentials */
#define EEPROM_CRED_START (EEPROM_RELAY_STATE_START + EEPROM_RELAY_STATE_SIZE)
#define EEPROM_CRED_SIZE 100

/* Genset config */
#define EEPROM_GENSET_START (EEPROM_CRED_START + EEPROM_CRED_SIZE)
#define EEPROM_GENSET_SIZE 12

/* Calibration (solo sensores físicos aprox) */
/* Slot layout must match CalibrationPersist in storage.cpp:
   magic(4)+version(2)+uid(4)+pers_state(1)+min(4)+max(4)+correction(4)+
   avail(1)+persist(1)+pulse(1)+pulse_ms(4)+fade(4) = 34 bytes */
#define EEPROM_CALIB_SLOT_SIZE 34
#define EEPROM_CALIB_START (EEPROM_GENSET_START + EEPROM_GENSET_SIZE)
#define EEPROM_CALIB_SIZE (MAX_PERSISTED_SENSORS * EEPROM_CALIB_SLOT_SIZE)

/* Automation rules */
#define EEPROM_RULES_START (EEPROM_CALIB_START + EEPROM_CALIB_SIZE)
#define EEPROM_RULES_SIZE 1600

/* =========================
   PROTECCION EEPROM
   ========================= */

#if (EEPROM_RULES_START + EEPROM_RULES_SIZE) > EEPROM_SIZE
#error EEPROM layout overflow
#endif

/* OTA flag + 4-byte device identity token, placed in the reserved area after
   the rules block. Non-aliased: does not overlap relay state (0..9) or
   credentials. An all-0xFF or all-0x00 slot means "unprovisioned". */
#define EEPROM_OTA_HASH_ADDR  (EEPROM_RULES_START + EEPROM_RULES_SIZE)
#define EEPROM_OTA_HASH_SIZE  4
#define EEPROM_OTA_FLAG_ADDR  (EEPROM_OTA_HASH_ADDR + EEPROM_OTA_HASH_SIZE)
#if (EEPROM_OTA_FLAG_ADDR + 1) > EEPROM_SIZE
#error OTA integrity region overflow
#endif

/* =========================
   RED
   ========================= */

#define AP_SSID "QymeraSetup"

#define BROADCAST_PORT 13345
#define COMMAND_PORT 13346

#define BROADCAST_INTERVAL 5000
#define WIFI_RETRY_INTERVAL 180000

/* =========================
   PWM ABSTRACTION (0-255, 8-bit)
   Backed by LEDC (native) via qhal, 1 kHz / 8-bit to match the historical
   Arduino core behavior.
   ========================= */

#define PWM_MAX_RAW 4095
#define PWM_MAX_OUT 255

inline void pwmSetup(uint8_t pin) {
  qhal_pwm_setup(pin);
}
inline void pwmWritePin(uint8_t pin, uint8_t value) {
  qhal_pwm_write(pin, value);
}
inline uint8_t pwmReadPin(uint8_t pin) {
  return qhal_pwm_read(pin);
}