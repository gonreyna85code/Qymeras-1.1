#include "config.h"
#include "mesh.h"
#include "core.h"
#include "sensors.h"

namespace mesh {

WiFiUDP udp;
ReportEntry reports[MAX_SENSORS];
float MIN_VAL = -50.0f;
float MAX_VAL = 150.0f;
static RemoteDevice remote_devices[MAX_SENSORS];
static int remote_device_count = 0;
static unsigned long last_cleanup = 0;
static WiFiUDP mesh_udp;
static SensorDiscoveryCallback sensor_callback = nullptr;
static CommandCallback command_callback = nullptr;

void init() {
  mesh_udp.begin(core::genset.broadcast_port);
}

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb) {
  sensor_callback = cb;
}

void setCommandCallback(CommandCallback cb) {
  command_callback = cb;
}

void tick(uint32_t now_ms) {
  int packet_size = mesh_udp.parsePacket();
  if (packet_size < (int)sizeof(PacketHeader)) {
    return;
  }  
  PacketHeader hdr;
  mesh_udp.read((uint8_t*)&hdr, sizeof(hdr));
  if (hdr.magic != 0xA5) return;
  if (hdr.version != 1 && hdr.version != PACKET_VERSION) return;
  if (hdr.size != packet_size) return;  
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);  
  String remote_ip = mesh_udp.remoteIP().toString();
  int remaining = hdr.size - sizeof(PacketHeader);
  int packet_len = (hdr.version == 1) ? sizeof(PacketV1) : sizeof(Packet);
  while (remaining >= packet_len) {
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (hdr.version == 1) {
      PacketV1 pkt_v1;
      mesh_udp.read((uint8_t*)&pkt_v1, sizeof(pkt_v1));
      pkt.id = pkt_v1.id;
      pkt.type = pkt_v1.type;
      pkt.value = pkt_v1.value;
      pkt.state = pkt_v1.state;
    } else {
      mesh_udp.read((uint8_t*)&pkt, sizeof(pkt));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
    }
    remaining -= packet_len;    
    if (is_remote) {
      if (sensor_callback) {
        sensor_callback(hdr.uid, remote_ip, pkt.id, String(pkt.name), pkt.type, pkt.state, pkt.value);
      }
      int idx = -1;
      for (int i = 0; i < remote_device_count; i++) {
        if (remote_devices[i].uid == hdr.uid) {
          idx = i;
          break;
        }
      }      
      if (idx == -1 && remote_device_count < MAX_SENSORS) {
        idx = remote_device_count++;
      }      
      if (idx >= 0) {
        remote_devices[idx].uid = hdr.uid;
        remote_devices[idx].ip = remote_ip;
        remote_devices[idx].last_seen = now_ms;
        remote_devices[idx].online = true;
      }
    } else {      
      if (command_callback) {
        command_callback(pkt.type, pkt.id, pkt.value, pkt.state);
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
RemoteDevice* getRemoteDevice(uint32_t uid) {
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
  RemoteDevice* dev = getRemoteDevice(uid);
  return dev != nullptr && dev->online;
}

void setReport(uint8_t index, uint32_t uid, float value, float raw, bool state) {
  if (index >= MAX_SENSORS)
    return;
  reports[index].uid = uid;
  reports[index].value = value;
  reports[index].raw = raw;
  reports[index].state = state;
}

uint32_t encodeFloat(float v) {
  if (v < MIN_VAL)
    v = MIN_VAL;
  if (v > MAX_VAL)
    v = MAX_VAL;
  return (uint32_t)((v - MIN_VAL) / (MAX_VAL - MIN_VAL) * 0xFFFFFFFF);
}

void sendBinaryReport() {
  PacketHeader hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  uint8_t count = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    if (c.local && c.type != sensors::SENSOR_NONE && c.uid != 0)
      count++;
  }
  uint16_t payloadSize = count * sizeof(Packet);
  hdr.size = sizeof(PacketHeader) + payloadSize;
  udp.beginPacket("255.255.255.255", core::genset.broadcast_port);
  udp.write((uint8_t *)&hdr, sizeof(hdr));
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    if (!c.local || c.type == sensors::SENSOR_NONE || c.uid == 0)
      continue;
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.id = c.uid;
    pkt.type = c.type;
    pkt.state = c.state ? 1 : 0;
    strncpy(pkt.name, c.id.c_str(), sizeof(pkt.name) - 1);
    if (c.type == sensors::SENSOR_LUMI) {
      pkt.value = (uint32_t)c.value;
    } else {
      pkt.value = encodeFloat(c.value);
    }
    udp.write((uint8_t *)&pkt, sizeof(pkt));
  }
  udp.endPacket();
}

}  // namespace mesh
