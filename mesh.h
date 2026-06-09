#pragma once
#include <WiFiUdp.h>
#include <Arduino.h>
#include "config.h"

namespace mesh {

// Inicialización
void init();

// ============================================================================
// Devices remotos
// ============================================================================

struct RemoteDevice {
  uint32_t uid;
  String ip;
  unsigned long last_seen;
  bool online;
};

// ============================================================================
// Callbacks
// ============================================================================

typedef void (*SensorDiscoveryCallback)(
    uint32_t device_uid,
    const String &device_ip,
    uint32_t sensor_id,
    const String &sensor_name,
    uint8_t sensor_type,
    bool sensor_state,
    uint32_t sensor_value);

typedef void (*CommandCallback)(
    uint8_t command_type,
    uint32_t sensor_id,
    uint32_t value,
    bool state);

// ============================================================================
// Reportes
// ============================================================================

struct ReportEntry {
  uint32_t uid;
  float value;
  float raw;
  bool state;
};

extern ReportEntry reports[MAX_SENSORS];
extern WiFiUDP udp;

// ============================================================================
// Protocolo
// ============================================================================

#pragma pack(push, 1)

static const uint8_t PACKET_VERSION = 2;
static const uint8_t SENSOR_NAME_LEN = 24;

struct PacketHeader {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
};

struct PacketV1 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
};

struct Packet {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
};

#pragma pack(pop)

// ============================================================================
// API pública
// ============================================================================

void setReport(
    uint8_t index,
    uint32_t uid,
    float value,
    float raw,
    bool state);

uint32_t encodeFloat(float v);

void tick(uint32_t now_ms);

void sendBinaryReport();

// ============================================================================
// Registro de callbacks
// ============================================================================

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb);

void setCommandCallback(CommandCallback cb);

// ============================================================================
// Utilidades
// ============================================================================

RemoteDevice *getRemoteDevice(uint32_t uid);

int getRemoteDeviceCount();

bool isDeviceOnline(uint32_t uid);

// ============================================================================
// Config
// ============================================================================

#define MESH_TIMEOUT 30000

} // namespace mesh