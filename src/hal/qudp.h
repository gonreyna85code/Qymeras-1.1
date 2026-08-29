#pragma once
/*
  qudp.h - minimal UDP socket wrapper over lwIP (qymera-IDF branch)

  Replaces Arduino's WiFiUDP in mesh.cpp with native BSD-socket code. Only the
  subset used by the mesh protocol v5 is implemented.
*/

#include <stdint.h>
#include <stddef.h>

#include "qhal.h"

#include <lwip/sockets.h>

class Qudp {
 public:
  Qudp();
  ~Qudp();

  // Bind to a local port (broadcast + reuseaddr enabled, non-blocking reads).
  bool begin(uint16_t port);

  // Build an outgoing datagram.
  void beginPacket(const char *host, uint16_t port);
  void write(const void *data, size_t len);
  size_t endPacket();

  // Non-blocking: returns payload size or <=0 when nothing is waiting.
  int parsePacket();
  int read(uint8_t *buf, size_t n);
  int available();
  QIP remoteIP();
  const char *remoteIPStr();      // cached dotted string of the last sender

  void close();

 private:
  int fd_ = -1;
  struct sockaddr_in tx_dst_;
  uint8_t rx_[1400];
  int rx_len_ = 0;
  int rx_off_ = 0;
  uint8_t tx_[1400];
  size_t tx_len_ = 0;
  QIP remote_;
  char remote_str_[16] = {0};
};