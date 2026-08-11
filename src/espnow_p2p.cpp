#include "espnow_p2p.h"
#include "log.h"

#if defined(ESP8266)
#include <espnow.h>
#elif defined(ESP32)
#include <esp_now.h>
#endif

namespace mesh {

static uint8_t peers[25][6];
static int peer_count = 0;
static bool espnow_ready = false;
static bool espnow_enabled = false;
static uint8_t rx_buf[250];
static volatile bool rx_ready = false;
static uint8_t rx_src[6];
static uint8_t rx_len = 0;

// ================= CALLBACKS =================

#if defined(ESP8266)
static void espnow_send_cb(uint8_t *mac, uint8_t status) {}
static void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len > 0 && len <= 250) {
    memcpy(rx_src, mac, 6);
    memcpy(rx_buf, data, len);
    rx_len = len;
    rx_ready = true;
  }
}
#elif defined(ESP32)
static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status) {}
static void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
  if (len > 0 && len <= 250) {
    memcpy(rx_src, mac, 6);
    memcpy(rx_buf, data, len);
    rx_len = len;
    rx_ready = true;
  }
}
#endif

// ================= API =================

bool espnow_init() {
  if (esp_now_init() == 0) {
#if defined(ESP8266)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#endif
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);
    espnow_ready = true;
    logger::core("ESP-NOW initialized");
    return true;
  }
  logger::error("ESP-NOW init failed");
  return false;
}

void espnow_send_broadcast(const uint8_t *data, uint16_t len) {
  if (!espnow_ready || !espnow_enabled) return;
  for (int i = 0; i < peer_count; i++) {
    esp_now_send(peers[i], (uint8_t *)data, len);
  }
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcast, (uint8_t *)data, len);
}

bool espnow_recv(uint8_t *buf, uint16_t *len, uint8_t *src_mac) {
  if (!rx_ready) return false;
  rx_ready = false;
  memcpy(buf, rx_buf, rx_len);
  *len = rx_len;
  memcpy(src_mac, rx_src, 6);
  return true;
}

void espnow_add_peer(const uint8_t *mac) {
  if (!espnow_ready) return;
  if (peer_count >= 25) return;
  for (int i = 0; i < peer_count; i++) {
    if (memcmp(peers[i], mac, 6) == 0) return;
  }
  memcpy(peers[peer_count], mac, 6);
#if defined(ESP8266)
  esp_now_add_peer(peers[peer_count], ESP_NOW_ROLE_COMBO, 1, NULL, 0);
#elif defined(ESP32)
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peers[peer_count], 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
#endif
  peer_count++;
  logger::coref("ESP-NOW peer added (%d total)", peer_count);
}

void espnow_clear_peers() {
  if (!espnow_ready) return;
  for (int i = 0; i < peer_count; i++) {
    esp_now_del_peer(peers[i]);
  }
  peer_count = 0;
}

uint8_t espnow_get_peer_count() {
  return peer_count;
}

void espnow_set_enabled(bool enabled) {
  espnow_enabled = enabled;
}

bool espnow_is_enabled() {
  return espnow_enabled;
}

}  // namespace mesh
