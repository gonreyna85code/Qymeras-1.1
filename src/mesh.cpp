#include "mesh.h"
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "log.h"

namespace mesh {

WiFiUDP udp;
ReportEntry reports[MAX_SENSORS];
float MIN_VAL = -50.0f;
float MAX_VAL = 150.0f;
static RemoteDevice remote_devices[MAX_SENSORS];
static int remote_device_count = 0;
static unsigned long last_cleanup = 0;
static WiFiUDP mesh_udp;
static WiFiUDP cmd_udp;
static SensorDiscoveryCallback sensor_callback = nullptr;
static CommandCallback command_cb = nullptr;

// ================= TRANSPORT =================
static Transport transport = TRANSPORT_UDP;

void setTransport(Transport t) {
  if (t == transport) return;
  transport = t;
  espnow_set_enabled(t == TRANSPORT_ESPNOW);
  logger::coref("Mesh transport: %s", t == TRANSPORT_ESPNOW ? "ESP-NOW" : "UDP");
}

Transport getTransport() {
  return transport;
}

void init() {
  mesh_udp.begin(core::genset.broadcast_port);
  udp.begin(core::genset.command_port);
  espnow_init();
}

static bool udpTxReady() {
#if defined(ESP32)
  return WiFi.getMode() != WIFI_MODE_NULL;
#else
  return true;
#endif
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
  if (hdr.version != 1 && hdr.version != 2 && hdr.version != PACKET_VERSION) return;
  if (hdr.size != len) return;
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);
  int remaining = hdr.size - sizeof(PacketHeader);
  int packet_len = (hdr.version == 1) ? sizeof(PacketV1) :
                   (hdr.version == 2) ? sizeof(PacketV2) :
                                        sizeof(Packet);
  const uint8_t *ptr = buf + sizeof(PacketHeader);

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
    } else {
      memcpy(&pkt, ptr, sizeof(pkt));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
    }
    ptr += packet_len;
    remaining -= packet_len;

    if (is_remote) {
      if (sensor_callback) {
        sensor_callback(
          hdr.uid, remote_ip,
          pkt.id, String(pkt.name),
          pkt.type, pkt.state,
          pkt.value, pkt.min, pkt.max,
          pkt.correction, pkt.avail);
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

static void parseUDPPacket(WiFiUDP &socket, uint32_t now_ms) {
  int packet_size = socket.parsePacket();
  if (packet_size < (int)sizeof(PacketHeader)) return;
  uint8_t buf[512];
  int len = socket.read(buf, sizeof(buf));
  if (len <= 0) return;
  char remote_ip[16];
  IPAddress rip = socket.remoteIP();
  snprintf(remote_ip, sizeof(remote_ip), "%d.%d.%d.%d", rip[0], rip[1], rip[2], rip[3]);
  parseBuffer(buf, len, remote_ip, now_ms);
}

// ================= TICK =================

void tick(uint32_t now_ms) {
  if (transport == TRANSPORT_UDP) {
    parseUDPPacket(mesh_udp, now_ms);
    parseUDPPacket(udp, now_ms);
  } else if (transport == TRANSPORT_ESPNOW) {
    uint8_t buf[250];
    uint16_t len;
    uint8_t src[6];
    while (espnow_recv(buf, &len, src)) {
      char mac_str[24];
      snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
        src[0], src[1], src[2], src[3], src[4], src[5]);
      parseBuffer(buf, len, mac_str, now_ms);
    }
  }
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
  PacketHeader hdr;
  hdr.magic   = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid     = GET_CHIP_ID();
  hdr.size    = sizeof(PacketHeader) + sizeof(Packet);
  Packet pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.id    = sensor_id;
  pkt.type  = type;
  pkt.value = value;
  pkt.state = state ? 1 : 0;

  uint8_t buf[sizeof(PacketHeader) + sizeof(Packet)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  if (transport == TRANSPORT_UDP) {
    cmd_udp.beginPacket(remote_ip, core::genset.command_port);
    cmd_udp.write(buf, sizeof(buf));
    cmd_udp.endPacket();
  } else if (espnow_is_enabled()) {
    espnow_send_broadcast(buf, sizeof(buf));
  }
}

void sendBinaryReport() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    if (!c.local || c.type == sensors::SENSOR_NONE || c.uid == 0) continue;
    PacketHeader hdr;
    hdr.magic = 0xA5;
    hdr.version = PACKET_VERSION;
    hdr.uid = GET_CHIP_ID();
    hdr.size = sizeof(PacketHeader) + sizeof(Packet);
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.id = c.uid;
    pkt.type = c.type;
    pkt.state = c.state ? 1 : 0;
    pkt.min = c.min;
    pkt.max = c.max;
    pkt.correction = c.correction;
    pkt.avail = c.avail;
    strncpy(pkt.name, c.name.c_str(), sizeof(pkt.name) - 1);
    if (c.type == sensors::SENSOR_LUMI) {
      pkt.value = (uint32_t)c.value;
    } else if (c.type == sensors::SENSOR_TIME) {
      pkt.value = (uint32_t)c.value;
    } else {
      pkt.value = encodeFloat(c.value);
    }
    uint8_t buf[sizeof(PacketHeader) + sizeof(Packet)];
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  if (transport == TRANSPORT_UDP) {
    if (udpTxReady()) {
      udp.beginPacket("255.255.255.255", core::genset.broadcast_port);
      udp.write(buf, sizeof(buf));
      udp.endPacket();
    }
  } else if (espnow_is_enabled()) {
    espnow_send_broadcast(buf, sizeof(buf));
  }
}
}

void sendLog(uint8_t layer, uint8_t level, const char *message) {
  PacketHeader hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  hdr.size = sizeof(PacketHeader) + sizeof(LogPacket);

  LogPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.layer = layer;
  pkt.level = level;
  strncpy(pkt.message, message, sizeof(pkt.message) - 1);

  uint8_t buf[sizeof(PacketHeader) + sizeof(LogPacket)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  if (transport == TRANSPORT_UDP) {
    if (udpTxReady()) {
      udp.beginPacket("255.255.255.255", core::genset.broadcast_port);
      udp.write(buf, sizeof(buf));
      udp.endPacket();
    }
  } else if (espnow_is_enabled()) {
    espnow_send_broadcast(buf, sizeof(buf));
  }
}

}  // namespace mesh
