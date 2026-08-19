#pragma once
#include <WiFiUdp.h>
#include <Arduino.h>
#include "config.h"
#include "espnow_p2p.h"

namespace mesh {

// ================= TRANSPORT MODE =================
enum Transport : uint8_t {
  TRANSPORT_UDP = 0,
  TRANSPORT_ESPNOW = 1
};

// Inicialización
void init();
void setTransport(Transport t);
Transport getTransport();

// ============================================================================
// Devices remotos
// ============================================================================

struct RemoteDevice {
  uint32_t uid;
  char ip[16];
  unsigned long last_seen;
  bool online;
};

// ============================================================================
// Callbacks
// ============================================================================

typedef void (*SensorDiscoveryCallback)(
  uint32_t device_uid,
  const char *device_ip,
  uint32_t sensor_id,
  const String &sensor_name,
  uint8_t sensor_type,
  bool sensor_state,
  uint32_t sensor_value,
  float sensor_min,
  float sensor_max,
  float sensor_correction,
  uint8_t sensor_avail);

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

static const uint8_t PACKET_VERSION = 4;
static const uint8_t SENSOR_NAME_LEN = 24;

/* Explicit payload kind. Required so LogPacket payloads are never parsed as
   sensor Packet payloads (they share the broadcast transport). */
enum PacketKind : uint8_t {
  PACKET_SENSOR = 1,
  PACKET_LOG = 2
};

/* Legacy base header (protocol v1/v2/v3, no kind byte). */
struct PacketHeader {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
};

/* Current header (protocol v4): adds the explicit kind byte. */
struct PacketHeaderV4 {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
  uint8_t kind;
};

struct PacketV1 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
};

struct PacketV2 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
};

struct Packet {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
  float min;
  float max;
  float correction;
  uint8_t avail;
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

// ============================
// LOG OVER UDP
// ============================

struct LogPacket {
  uint8_t layer;
  uint8_t level;
  char message[64];
};

void sendLog(uint8_t layer, uint8_t level, const char *message);

// ============================================================================
// Registro de callbacks
// ============================================================================

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb);

void setCommandCallback(CommandCallback cb);

void sendCommand(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t sensor_id,
  uint8_t type,
  uint32_t value,
  bool state);

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

}  // namespace mesh