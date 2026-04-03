#pragma once
#include <Arduino.h>

/* =========================
   LIMITES DEL SISTEMA
   ========================= */

#define MAX_SENSORS 64
#define MAX_RULES   20


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
#define COMMAND_PORT   13346

#define BROADCAST_INTERVAL 40000
#define WIFI_RETRY_INTERVAL 180000