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

void init() {
  mesh_udp.begin(BROADCAST_PORT);
}

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb) {
  sensor_callback = cb;
}

void tick(uint32_t now_ms) {
  // Recibir broadcasts que otros devices están enviando
  int packet_size = mesh_udp.parsePacket();
  if (packet_size > (int)sizeof(core::PacketHeader)) {
    uint8_t buffer[packet_size];
    mesh_udp.read(buffer, packet_size);
    
    // Parsear header
    core::PacketHeader* hdr = (core::PacketHeader*)buffer;
    
    // Si el magic y version coinciden y es de otro device
    uint32_t local_uid = GET_CHIP_ID();
    if (hdr->magic == 0xA5 && hdr->version == 1 && hdr->uid != local_uid) {
      String remote_ip = mesh_udp.remoteIP().toString();
      
      // Buscar o crear entrada en tabla de devices
      int idx = -1;
      for (int i = 0; i < remote_device_count; i++) {
        if (remote_devices[i].uid == hdr->uid) {
          idx = i;
          break;
        }
      }
      
      if (idx == -1 && remote_device_count < 10) {
        idx = remote_device_count++;
      }
      
      if (idx >= 0) {
        remote_devices[idx].uid = hdr->uid;
        remote_devices[idx].ip = remote_ip;
        remote_devices[idx].last_seen = now_ms;
        remote_devices[idx].online = true;
        
        // Parsear paquetes de sensores y llamar callback
        int offset = sizeof(core::PacketHeader);
        int packets_count = (packet_size - offset) / (int)sizeof(core::Packet);
        
        for (int p = 0; p < packets_count; p++) {
          core::Packet* pkt = (core::Packet*)(buffer + offset + p * sizeof(core::Packet));
          
          // Llamar callback inyectable (sensor.cpp lo maneja)
          if (sensor_callback) {
            sensor_callback(hdr->uid, remote_ip, pkt->id, pkt->type, pkt->state, pkt->value);
          }
        }
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
