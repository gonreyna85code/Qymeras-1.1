#pragma once
#include <Arduino.h>
#include "config.h"

namespace mesh {

bool espnow_init();
void espnow_send_broadcast(const uint8_t *data, uint16_t len);
bool espnow_recv(uint8_t *buf, uint16_t *len, uint8_t *src_mac);
// RX queue metrics: total dropped messages (queue full) and current depth.
uint32_t espnow_get_rx_overflow();
uint8_t espnow_get_rx_queue_depth();
void espnow_add_peer(const uint8_t *mac);
void espnow_clear_peers();
uint8_t espnow_get_peer_count();
void espnow_set_enabled(bool enabled);
bool espnow_is_enabled();

}  // namespace mesh
