#include "config.h"
#include "mesh.h"
#include "core.h"
#include <WiFiUdp.h>

namespace mesh {

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
  // Recibir y parsear paquetes UDP
  int packet_size = mesh_udp.parsePacket();
  if (packet_size < (int)sizeof(core::PacketHeader)) {
    return;
  }
  
  core::PacketHeader hdr;
  mesh_udp.read((uint8_t*)&hdr, sizeof(hdr));
  
  // Validar header
  if (hdr.magic != 0xA5) return;
  if (hdr.version != 1 && hdr.version != core::PACKET_VERSION) return;
  if (hdr.size != packet_size) return;
  
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);
  
  String remote_ip = mesh_udp.remoteIP().toString();
  
  // Procesar paquetes
  int remaining = hdr.size - sizeof(core::PacketHeader);
  int packet_len = (hdr.version == 1) ? sizeof(core::PacketV1) : sizeof(core::Packet);
  while (remaining >= packet_len) {
    core::Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (hdr.version == 1) {
      core::PacketV1 pkt_v1;
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
      // ===== PRESENCIA REMOTA =====
      // Llamar sensor discovery callback
      if (sensor_callback) {
        sensor_callback(hdr.uid, remote_ip, pkt.id, String(pkt.name), pkt.type, pkt.state, pkt.value);
      }
      
      // Actualizar tabla de devices remotos
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
      // ===== COMANDO LOCAL =====
      // Llamar command callback
      if (command_callback) {
        command_callback(pkt.type, pkt.id, pkt.value, pkt.state);
      }
    }
  }
  
  // Limpiar devices offline cada 30 segundos
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

}  // namespace mesh
