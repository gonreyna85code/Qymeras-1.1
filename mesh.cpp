#include "mesh.h"
#include "config.h"
#include "core.h"
#include <WiFi.h>
#include <WiFiUdp.h>

namespace mesh {

static RemoteDevice remote_devices[10];
static int remote_device_count = 0;
static unsigned long last_cleanup = 0;
static WiFiUDP mesh_udp;
static SensorDiscoveryCallback sensor_callback = nullptr;
static CommandCallback command_callback = nullptr;

void init() {
  mesh_udp.begin(BROADCAST_PORT);
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
  if (hdr.version != 1) return;
  if (hdr.size != packet_size) return;
  
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);
  
  String remote_ip = mesh_udp.remoteIP().toString();
  
  // Procesar paquetes
  int remaining = hdr.size - sizeof(core::PacketHeader);
  while (remaining >= (int)sizeof(core::Packet)) {
    core::Packet pkt;
    mesh_udp.read((uint8_t*)&pkt, sizeof(pkt));
    remaining -= sizeof(core::Packet);
    
    if (is_remote) {
      // ===== PRESENCIA REMOTA =====
      // Llamar sensor discovery callback
      if (sensor_callback) {
        sensor_callback(hdr.uid, remote_ip, pkt.id, pkt.type, pkt.state, pkt.value);
      }
      
      // Actualizar tabla de devices remotos
      int idx = -1;
      for (int i = 0; i < remote_device_count; i++) {
        if (remote_devices[i].uid == hdr.uid) {
          idx = i;
          break;
        }
      }
      
      if (idx == -1 && remote_device_count < 10) {
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
