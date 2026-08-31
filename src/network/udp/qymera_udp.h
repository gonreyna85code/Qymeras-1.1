/**
 * Qymera Dashboard - UDP Protocol Foundation
 * Core UDP transport for device communication
 */
#pragma once

#include "qymera_types.h"
#include "qymera_ring.h"
#include "qymera_hal.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Protocol v6 Message Types
 * ========================= */

typedef enum {
    QYMERA_MSG_NONE = 0,
    QYMERA_MSG_DISCOVER = 0x01,
    QYMERA_MSG_REGISTER = 0x02,
    QYMERA_MSG_ANNOUNCE = 0x03,
    QYMERA_MSG_ENTITY_SAMPLE = 0x04,
    QYMERA_MSG_ENTITY_STATE = 0x05,
    QYMERA_MSG_COMMAND = 0x06,
    QYMERA_MSG_ACK = 0x07,
    QYMERA_MSG_PING = 0x08,
    QYMERA_MSG_PONG = 0x09,
    QYMERA_MSG_LOG = 0x0A,
} qymera_msg_type_t;

/* =========================
 * Message Header (v6)
 * ========================= */

#define QYMERA_PROTOCOL_MAGIC 0xA6
#define QYMERA_PROTOCOL_VERSION 6

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint8_t kind;
    uint32_t seq;
    uint32_t src_uid;
    uint16_t len;
} __attribute__((packed)) qymera_msg_header_t;

/* =========================
 * Payload Structures
 * ========================= */

typedef struct {
    uint32_t chip_uid;
    char model[32];
    char fw_version[32];
    uint32_t capability_mask;
    uint32_t entity_roster_hash;
} __attribute__((packed)) qymera_payload_register_t;

typedef struct {
    uint32_t entity_id;
    uint8_t type;
    uint8_t flags;
    float value_f;
    uint32_t value_u32;
    uint32_t src_ts;
    int8_t rssi;
} __attribute__((packed)) qymera_payload_entity_sample_t;

typedef struct {
    uint32_t entity_id;
    uint8_t opcode;
    uint8_t flags;
    float value_f;
    uint32_t value_u32;
    uint32_t cmd_seq;
} __attribute__((packed)) qymera_payload_command_t;

typedef struct {
    uint32_t cmd_seq;
    uint8_t result;
} __attribute__((packed)) qymera_payload_ack_t;

/* =========================
 * UDP Transport Config
 * ========================= */

typedef struct {
    qymera_udp_socket_t discovery_sock;
    qymera_udp_socket_t control_sock;
    qymera_ring_t rx_ring;
    qymera_ring_t tx_ring;
    uint32_t local_uid;
    uint32_t tx_seq;
} qymera_udp_transport_config_t;

/* =========================
 * UDP Transport Handle
 * ========================= */

typedef struct qymera_udp_transport_s qymera_udp_transport_t;

/* =========================
 * UDP Transport API
 * ========================= */

qymera_err_t qymera_udp_transport_init(qymera_udp_transport_t **transport, const qymera_udp_transport_config_t *config);
size_t qymera_udp_transport_receive(qymera_udp_transport_t *transport);
qymera_err_t qymera_udp_transport_send(qymera_udp_transport_t *transport, qymera_msg_type_t msg_type,
                                        const char *dest_ip, uint16_t dest_port,
                                        const void *payload, size_t payload_len);
qymera_err_t qymera_udp_transport_send_command(qymera_udp_transport_t *transport, const char *dest_ip,
                                                uint32_t entity_id, uint8_t opcode,
                                                float value_f, uint32_t value_u32,
                                                uint32_t *cmd_seq);

typedef qymera_err_t (*qymera_udp_msg_cb_t)(qymera_udp_transport_t *transport,
                                            const qymera_msg_header_t *header,
                                            const void *payload, size_t payload_len,
                                            const char *src_ip, uint16_t src_port);

void qymera_udp_transport_set_callback(qymera_udp_transport_t *transport, qymera_msg_type_t msg_type,
                                        qymera_udp_msg_cb_t callback, void *context);

void qymera_udp_transport_get_stats(qymera_udp_transport_t *transport, qymera_ring_stats_t *rx_stats, qymera_ring_stats_t *tx_stats);

#ifdef __cplusplus
}
#endif