#pragma once
/*
  Qymera.h - Master library header for Arduino IDE sketches.

  The user sketch must implement:
    - void initSatellite()       : initialize hardware libraries (Wire, etc.)
    - void report()              : read hardware and report values via sensors::xxx()
    - void onCommandHook(...)    : custom logic for received commands
  The library handles: WiFi, web server, UDP mesh, automations, EEPROM.
*/
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "mesh.h"
#include "web.h"
#include "automations.h"
#include "log.h"
#include "ai.h"
