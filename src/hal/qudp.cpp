/* qudp.cpp - minimal UDP socket wrapper over lwIP (qymera-IDF branch) */

#include "qudp.h"

#include <stdio.h>
#include <string.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"
#include "lwip/fcntl.h"

Qudp::Qudp() {
  close();
}

Qudp::~Qudp() {
  close();
}

void Qudp::close() {
  if (fd_ >= 0) {
    lwip_close(fd_);
    fd_ = -1;
  }
  rx_len_ = 0;
  rx_off_ = 0;
  tx_len_ = 0;
  memset(&remote_, 0, sizeof(remote_));
}

bool Qudp::begin(uint16_t port) {
  if (fd_ >= 0) close();

  fd_ = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd_ < 0) return false;

  int one = 1;
  lwip_setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  lwip_setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(fd_, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close();
    return false;
  }

  int flags = lwip_fcntl(fd_, F_GETFL, 0);
  lwip_fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
  return true;
}

void Qudp::beginPacket(const char *host, uint16_t port) {
  tx_len_ = 0;
  tx_dst_ = {};
  tx_dst_.sin_family = AF_INET;
  tx_dst_.sin_port = htons(port);
  if (host) {
    inet_pton(AF_INET, host, &tx_dst_.sin_addr);
  } else {
    tx_dst_.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  }
}

void Qudp::write(const void *data, size_t len) {
  if (tx_len_ + len > sizeof(tx_)) len = sizeof(tx_) - tx_len_;
  memcpy(tx_ + tx_len_, data, len);
  tx_len_ += len;
}

size_t Qudp::endPacket() {
  if (fd_ < 0 || tx_len_ == 0) return 0;
  int sent = lwip_sendto(fd_, tx_, tx_len_, 0,
                         (struct sockaddr *)&tx_dst_, sizeof(tx_dst_));
  tx_len_ = 0;
  return (sent < 0) ? 0 : (size_t)sent;
}

int Qudp::parsePacket() {
  if (fd_ < 0) return 0;
  struct sockaddr_in from = {};
  socklen_t from_len = sizeof(from);
  int n = lwip_recvfrom(fd_, rx_, sizeof(rx_), 0,
                        (struct sockaddr *)&from, &from_len);
  rx_off_ = 0;
  if (n <= 0) {
    rx_len_ = 0;
    return (n == 0) ? 0 : 0;  // EAGAIN / errors are both "nothing ready"
  }
  rx_len_ = n;
  uint32_t a = ntohl(from.sin_addr.s_addr);
  remote_.b[0] = (a >> 24) & 0xFF;
  remote_.b[1] = (a >> 16) & 0xFF;
  remote_.b[2] = (a >> 8) & 0xFF;
  remote_.b[3] = a & 0xFF;
  snprintf(remote_str_, sizeof(remote_str_), "%u.%u.%u.%u",
           remote_.b[0], remote_.b[1], remote_.b[2], remote_.b[3]);
  return n;
}

int Qudp::read(uint8_t *buf, size_t n) {
  int avail = rx_len_ - rx_off_;
  if (avail <= 0) return 0;
  if ((int)n > avail) n = avail;
  memcpy(buf, rx_ + rx_off_, n);
  rx_off_ += (int)n;
  return (int)n;
}

int Qudp::available() {
  return rx_len_ - rx_off_;
}

QIP Qudp::remoteIP() {
  return remote_;
}

const char *Qudp::remoteIPStr() {
  return remote_str_;
}