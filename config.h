#pragma once
#include <Arduino.h>

/* =========================
   PLATAFORMA & COMPATIBILIDAD
   ========================= */

// Auto-detección
#if defined(ESP8266)
#define PLATFORM_ESP8266 1
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
typedef ESP8266WebServer WebServerCompat;
#define ICACHE_FLASH ICACHE_FLASH_ATTR
#define GET_CHIP_ID() ESP.getChipId()
#define SET_WIFI_SLEEP() WiFi.setSleepMode(WIFI_NONE_SLEEP)
#define SET_AUTO_CONNECT() WiFi.setAutoConnect(true)
#define RESET_MCU() ESP.reset()

#elif defined(ESP32)
#define PLATFORM_ESP32 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32S2)
#define PLATFORM_ESP32S2 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32S3)
#define PLATFORM_ESP32S3 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32C3)
#define PLATFORM_ESP32C3 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#else
#error "Plataforma no soportada. Usa: ESP8266, ESP32, ESP32S2, ESP32S3, ESP32C3"
#endif

/* =========================
   LIMITES DEL SISTEMA
   ========================= */

#define MAX_SENSORS 64
#define MAX_RULES 20

/* =========================
   EEPROM
   ========================= */

#define EEPROM_SIZE 4096

/* Relay state */
#define EEPROM_RELAY_STATE_START 0
#define EEPROM_RELAY_STATE_SIZE 10

/* WiFi credentials */
#define EEPROM_CRED_START (EEPROM_RELAY_STATE_START + EEPROM_RELAY_STATE_SIZE)
#define EEPROM_CRED_SIZE 100

/* Genset config */
#define EEPROM_GENSET_START (EEPROM_CRED_START + EEPROM_CRED_SIZE)
#define EEPROM_GENSET_SIZE 12

/* Calibration (solo sensores físicos aprox) */
#define EEPROM_CALIB_START (EEPROM_GENSET_START + EEPROM_GENSET_SIZE)
#define EEPROM_CALIB_SIZE 512

/* Automation rules */
#define EEPROM_RULES_START (EEPROM_CALIB_START + EEPROM_CALIB_SIZE)
#define EEPROM_RULES_SIZE 1600

/* =========================
   PROTECCION EEPROM
   ========================= */

#if (EEPROM_RULES_START + EEPROM_RULES_SIZE) > EEPROM_SIZE
#error EEPROM layout overflow
#endif

/* =========================
   RED
   ========================= */

#define AP_SSID "PeriferalSetup"

#define BROADCAST_PORT 13345
#define COMMAND_PORT 13346

#define BROADCAST_INTERVAL 40000
#define WIFI_RETRY_INTERVAL 180000
