#include "mesh.h"
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "log.h"

namespace mesh {

// Wire-format guards: any change to the packed structs that alters their size
// must be reviewed against parseBuffer/senders (protocol compatibility).
static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");
static_assert(sizeof(PacketHeaderV4) == 9, "PacketHeaderV4 must be 9 bytes");
static_assert(sizeof(PacketV1) == 10, "PacketV1 must be 10 bytes");
static_assert(sizeof(PacketV2) == 34, "PacketV2 must be 34 bytes");
static_assert(sizeof(PacketV4) == 47, "PacketV4 must be 47 bytes");
static_assert(sizeof(Packet) == 58, "Packet must be 58 bytes");
static_assert(sizeof(LogPacket) == 66, "LogPacket must be 66 bytes");

Qudp udp;
ReportEntry reports[MAX_SENSORS];
float MIN_VAL = -50.0f;
float MAX_VAL = 150.0f;
static RemoteDevice remote_devices[MAX_SENSORS];
static int remote_device_count = 0;
static unsigned long last_cleanup = 0;
static Qudp mesh_udp;
static Qudp cmd_udp;
static SensorDiscoveryCallback sensor_callback = nullptr;
static CommandCallback command_cb = nullptr;

// ================= TRANSPORT =================
// Native ESP-IDF port carries mesh traffic exclusively over UDP. The historical
// ESP-NOW transport and espnow_p2p.* live on the 'main' (Arduino) branch and
// are not built here. TRANSPORT_ESPNOW is kept in the enum only for source
// compatibility; selecting it is a no-op with a warning.
static Transport transport = TRANSPORT_UDP;

void setTransport(Transport t) {
  if (t == transport) return;
  if (t != TRANSPORT_UDP) {
    logger::warnf("Mesh transport %u not available on ESP-IDF; keeping UDP", (uint8_t)t);
    return;
  }
  transport = t;
  logger::coref("Mesh transport: UDP");
}

Transport getTransport() {
  return transport;
}

void init() {
  mesh_udp.begin(core::genset.broadcast_port);
  udp.begin(core::genset.command_port);
}

static bool udpTxReady() {
  return qhal_wifi_init_done() &&
         (qhal_wifi_ap_active() || qhal_wifi_sta_connected());
}

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb) {
  sensor_callback = cb;
}

void setCommandCallback(CommandCallback cb) {
  command_cb = cb;
}

// ================= BUFFER PARSER (shared) =================

static void parseBuffer(const uint8_t *buf, uint16_t len, const char *remote_ip, uint32_t now_ms) {
  if (len < sizeof(PacketHeader)) return;
  PacketHeader hdr;
  memcpy(&hdr, buf, sizeof(hdr));
  if (hdr.magic != 0xA5) return;
  if (hdr.version < 1 || hdr.version > PACKET_VERSION) return;
  if (hdr.size != len) return;
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);

  // Protocol v4/v5 carry an explicit packet kind byte. Legacy v1/v2/v3 have no
  // kind byte and are always sensor packets.
  int header_size = sizeof(PacketHeader);
  PacketKind kind = PACKET_SENSOR;
  if (hdr.version == 4 || hdr.version == PACKET_VERSION) {
    if (len < header_size + 1) return;
    kind = (PacketKind)buf[header_size];
    header_size += 1;
    if (kind != PACKET_SENSOR && kind != PACKET_LOG) {
      logger::warnf("Mesh: unknown packet kind %u rejected", (uint8_t)kind);
      return;
    }
  }

  int remaining = hdr.size - header_size;
  if (remaining <= 0) return;

  // ---- Log payload: must NEVER reach sensor_callback() ----
  if (kind == PACKET_LOG) {
    if (remaining != (int)sizeof(LogPacket)) return;  // exact-size validation
    LogPacket lp;
    memcpy(&lp, buf + header_size, sizeof(lp));
    lp.message[sizeof(lp.message) - 1] = '\0';
    if (is_remote && lp.layer <= logger::EVENTS && lp.level <= logger::ERROR) {
      // Ingest into the local log GUI/serial buffer WITHOUT re-broadcasting
      // (prevents a broadcast ping-pong loop between devices).
      logger::logRemote((logger::Layer)lp.layer, (logger::Level)lp.level, lp.message);
    }
    return;
  }

  // ---- Sensor payload (v4/v5 PACKET_SENSOR, or legacy v1/v2/v3) ----
  int packet_len = (hdr.version == 1) ? sizeof(PacketV1) :
                   (hdr.version == 2) ? sizeof(PacketV2) :
                   (hdr.version <= 4) ? sizeof(PacketV4) :
                                        sizeof(Packet);
  // Exact-multiple validation: a payload whose size is not a whole number of
  // sensor packets is rejected. Legacy v3 log packets (66-byte payloads) fail
  // this check and are dropped instead of being fragmented into fake sensors.
  if (remaining % packet_len != 0) return;
  const uint8_t *ptr = buf + header_size;
  int parsed_packets = 0;

  while (remaining >= packet_len) {
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (hdr.version == 1) {
      PacketV1 pkt_v1;
      memcpy(&pkt_v1, ptr, sizeof(pkt_v1));
      pkt.id    = pkt_v1.id;
      pkt.type  = pkt_v1.type;
      pkt.value = pkt_v1.value;
      pkt.state = pkt_v1.state;
      pkt.min = 0; pkt.max = 100; pkt.correction = 0; pkt.avail = 0;
      pkt.name[0] = '\0';
    } else if (hdr.version == 2) {
      PacketV2 pkt_v2;
      memcpy(&pkt_v2, ptr, sizeof(pkt_v2));
      pkt.id    = pkt_v2.id;
      pkt.type  = pkt_v2.type;
      pkt.value = pkt_v2.value;
      pkt.state = pkt_v2.state;
      memcpy(pkt.name, pkt_v2.name, sizeof(pkt.name));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
      pkt.min = 0; pkt.max = 100; pkt.correction = 0; pkt.avail = 0;
    } else if (hdr.version == 3 || hdr.version == 4) {
      // Legacy 47-byte v3/v4 packet: copy into the 58-byte v5 layout. The extra
      // config fields (fade/persist/pers_state/pulse/pulse_ms) stay 0 because
      // the legacy peer did not transmit them.
      PacketV4 pkt_v4;
      memcpy(&pkt_v4, ptr, sizeof(pkt_v4));
      pkt.id        = pkt_v4.id;
      pkt.type      = pkt_v4.type;
      pkt.value     = pkt_v4.value;
      pkt.state     = pkt_v4.state;
      memcpy(pkt.name, pkt_v4.name, sizeof(pkt.name));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
      pkt.min       = pkt_v4.min;
      pkt.max       = pkt_v4.max;
      pkt.correction = pkt_v4.correction;
      pkt.avail     = pkt_v4.avail;
    } else {
      memcpy(&pkt, ptr, sizeof(pkt));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
    }
    ptr += packet_len;
    remaining -= packet_len;
    parsed_packets++;

    if (is_remote) {
      if (sensor_callback) {
        sensor_callback(
          hdr.uid, remote_ip,
          pkt.id, String(pkt.name),
          pkt.type, pkt.state,
          pkt.value, pkt.min, pkt.max,
          pkt.correction, pkt.avail,
          pkt.fade, pkt.persist, pkt.pers_state, pkt.pulse, pkt.pulse_ms);
      }
      int idx = -1;
      for (int i = 0; i < remote_device_count; i++) {
        if (remote_devices[i].uid == hdr.uid) { idx = i; break; }
      }
      if (idx == -1 && remote_device_count < MAX_SENSORS) {
        idx = remote_device_count++;
      }
      if (idx >= 0) {
        remote_devices[idx].uid       = hdr.uid;
        strncpy(remote_devices[idx].ip, remote_ip, sizeof(remote_devices[idx].ip) - 1);
        remote_devices[idx].ip[sizeof(remote_devices[idx].ip) - 1] = '\0';
        remote_devices[idx].last_seen = now_ms;
        remote_devices[idx].online    = true;
      }
    } else {
      if (command_cb) {
        command_cb(pkt.type, pkt.id, pkt.value, pkt.state != 0);
      }
    }
  }
  if (now_ms - last_cleanup > 30000) {
    last_cleanup = now_ms;
    for (int i = 0; i < remote_device_count; i++) {
      if (now_ms - remote_devices[i].last_seen > MESH_TIMEOUT) {
        remote_devices[i].online = false;
      }
    }
  }
}

// ================= UDP PARSER =================

static void parseUDPPacket(Qudp &socket, uint32_t now_ms) {
  // RX buffer must hold the largest configured discovery batch.
  static uint8_t buf[DISCOVERY_MAX_UDP_PACKET];
  int processed = 0;
  int packet_size;
  // Drain up to MAX_RX_PACKETS_PER_TICK datagrams per socket per tick. A UDP
  // storm must not monopolize loop(); leftovers are handled on the next tick().
  while (processed < MAX_RX_PACKETS_PER_TICK && (packet_size = socket.parsePacket()) > 0) {
    // Reject oversized/malformed datagrams: a payload larger than our buffer
    // cannot be fully drained and would otherwise leave stale bytes that
    // parsePacket() re-yields every tick -> an infinite spin. Drop-and-drain.
    if (packet_size > (int)sizeof(buf)) {
      while (socket.available()) socket.read();
      break;
    }
    int len = socket.read(buf, sizeof(buf));
    if (len <= 0) {
      // parsePacket() reported a datagram but read() could not retrieve it
      // (e.g. WiFi reconnecting, socket in a transitional state). Drain the
      // pending bytes and stop this tick; retry on the next tick. This prevents
      // the loop from re-yielding the same unreadable datagram forever.
      while (socket.available()) socket.read();
      break;
    }
    const char *rip = socket.remoteIPStr();
    parseBuffer(buf, len, rip ? rip : "0.0.0.0", now_ms);
    // Guarantee the datagram is fully dequeued. On legacy Arduino a socket can
    // re-yield the same datagram across ticks if the leading read() does not
    // consume it entirely, which would spin loop() at full speed. lwIP sockets
    // drain atomically per recvfrom(), but the drain is kept as a cheap guard.
    while (socket.available()) socket.read();
    processed++;
  }
}

// ================= TICK =================

void tick(uint32_t now_ms) {
  parseUDPPacket(mesh_udp, now_ms);
  parseUDPPacket(udp, now_ms);
}

// ================= DEVICES =================

RemoteDevice *getRemoteDevice(uint32_t uid) {
  for (int i = 0; i < remote_device_count; i++) {
    if (remote_devices[i].uid == uid) {
      return &remote_devices[i];
    }
  }
  return nullptr;
}

int getRemoteDeviceCount() {
  return remote_device_count;
}

bool isDeviceOnline(uint32_t uid) {
  RemoteDevice *dev = getRemoteDevice(uid);
  return dev != nullptr && dev->online;
}

void setReport(uint8_t index, uint32_t uid, float value, float raw, bool state) {
  if (index >= MAX_SENSORS) return;
  reports[index].uid = uid;
  reports[index].value = value;
  reports[index].raw = raw;
  reports[index].state = state;
}

uint32_t encodeFloat(float v) {
  if (v < MIN_VAL) v = MIN_VAL;
  if (v > MAX_VAL) v = MAX_VAL;
  return (uint32_t)((v - MIN_VAL) / (MAX_VAL - MIN_VAL) * 0xFFFFFFFF);
}

// ================= SEND =================

void sendCommand(uint32_t remote_uid, const char *remote_ip, uint32_t sensor_id, uint8_t type, uint32_t value, bool state) {
  if (!getRemoteDevice(remote_uid)) return;
  PacketHeaderV4 hdr;
  hdr.magic   = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid     = GET_CHIP_ID();
  hdr.kind    = PACKET_SENSOR;
  hdr.size    = sizeof(PacketHeaderV4) + sizeof(Packet);
  Packet pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.id    = sensor_id;
  pkt.type  = type;
  pkt.value = value;
  pkt.state = state ? 1 : 0;

  uint8_t buf[sizeof(PacketHeaderV4) + sizeof(Packet)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  if (transport == TRANSPORT_UDP) {
    cmd_udp.beginPacket(remote_ip, core::genset.command_port);
    cmd_udp.write(buf, sizeof(buf));
    cmd_udp.endPacket();
  }
}

static void fillPacket(const sensors::Calibration &c, Packet &pkt) {
  memset(&pkt, 0, sizeof(pkt));
  pkt.id = c.uid;
  pkt.type = c.type;
  pkt.state = c.state ? 1 : 0;
  pkt.min = c.min;
  pkt.max = c.max;
  pkt.correction = c.correction;
  pkt.avail = c.avail;
  pkt.fade = c.fade;
  pkt.persist = c.persist ? 1 : 0;
  pkt.pers_state = c.pers_state ? 1 : 0;
  pkt.pulse = c.pulse ? 1 : 0;
  pkt.pulse_ms = c.pulse_ms;
  strncpy(pkt.name, c.name.c_str(), sizeof(pkt.name) - 1);
  if (c.type == sensors::SENSOR_LUMI || c.type == sensors::SENSOR_TIME) {
    pkt.value = (uint32_t)c.value;
  } else {
    pkt.value = encodeFloat(c.value);
  }
}

// Send one UDP discovery batch: a v4 header followed by sensor_count Packets.
// The receiver already parses N packets per datagram (remaining % packet_len).
static void sendUdpBatch(uint8_t *buf, int sensor_count) {
  if (sensor_count <= 0) return;
  PacketHeaderV4 hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  hdr.kind = PACKET_SENSOR;
  hdr.size = sizeof(PacketHeaderV4) + sensor_count * sizeof(Packet);
  memcpy(buf, &hdr, sizeof(hdr));
  if (udpTxReady()) {
    udp.beginPacket("255.255.255.255", core::genset.broadcast_port);
    udp.write(buf, hdr.size);
    udp.endPacket();
  }
}

void sendBinaryReport() {
  // Batching: group local entities into datagrams of up to
  // DISCOVERY_MAX_UDP_PACKET bytes. Only local entities are announced.
  static uint8_t batch[DISCOVERY_MAX_UDP_PACKET];
  int sensor_count = 0;
  const int header_size = sizeof(PacketHeaderV4);
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    if (!c.local || c.type == sensors::SENSOR_NONE || c.uid == 0) continue;
    if (sensor_count > 0 &&
        header_size + (sensor_count + 1) * sizeof(Packet) > DISCOVERY_MAX_UDP_PACKET) {
      sendUdpBatch(batch, sensor_count);
      sensor_count = 0;
    }
    Packet pkt;
    fillPacket(c, pkt);
    memcpy(batch + header_size + sensor_count * sizeof(Packet), &pkt, sizeof(pkt));
    sensor_count++;
  }
  sendUdpBatch(batch, sensor_count);
}

void sendLog(uint8_t layer, uint8_t level, const char *message) {
  PacketHeaderV4 hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  hdr.kind = PACKET_LOG;
  hdr.size = sizeof(PacketHeaderV4) + sizeof(LogPacket);

  LogPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.layer = layer;
  pkt.level = level;
  strncpy(pkt.message, message, sizeof(pkt.message) - 1);

  uint8_t buf[sizeof(PacketHeaderV4) + sizeof(LogPacket)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  if (transport == TRANSPORT_UDP) {
    if (udpTxReady()) {
      udp.beginPacket("255.255.255.255", core::genset.broadcast_port);
      udp.write(buf, sizeof(buf));
      udp.endPacket();
    }
  }
}

}  // namespace mesh
