#pragma once
#include <Arduino.h>
#include "config.h"

namespace mesh {

// Estructura para representar un device remoto
struct RemoteDevice {
  uint32_t uid;                // Chip ID único
  String ip;                   // IP local del device
  unsigned long last_seen;     // Timestamp último broadcast
  bool online;                 // true = visto en último intervalo
};

// Callback inyectable para procesar sensores descubiertos
typedef void (*SensorDiscoveryCallback)(
  uint32_t device_uid,
  const String &device_ip,
  uint8_t sensor_id,
  uint8_t sensor_type,
  bool sensor_state,
  uint32_t sensor_value
);

// Discovery y sincronización
void init();                                          // Inicializa mesh listener
void tick(uint32_t now_ms);                          // Tick principal (discovery, cleanup)
void setSensorDiscoveryCallback(SensorDiscoveryCallback cb);  // Inyector de callback

// Utilidades
RemoteDevice* getRemoteDevice(uint32_t uid);
int getRemoteDeviceCount();
bool isDeviceOnline(uint32_t uid);

// Constants
#define MESH_TIMEOUT 30000  // Device offline si no se ve en 30 seg

}  // namespace mesh
