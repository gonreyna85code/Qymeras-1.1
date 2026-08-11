#pragma once
/*
  AntiMatterSatellite.h - Header maestro de la librería
  Exposición única para sketches de Arduino IDE.

  El sketch de usuario debe implementar:
    - void initSatellite()       : registrar sensores/actuadores locales
    - void report()              : leer hardware y enviar valores vía sensors::xxx()
    - void onCommandHook(...)    : lógica personalizada de comandos recibidos
*/
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "mesh.h"
#include "web.h"
#include "automations.h"
