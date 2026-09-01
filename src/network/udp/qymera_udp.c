/**
 * Qymera Dashboard - UDP Transport Implementation
 */
#include "qymera_udp.h"
#include "qymera_hal.h"
#include "qymera_log.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Define the socket struct locally since it's opaque in the header */
struct qymera_udp_socket {
    int sockfd;
    bool broadcast;
};

struct qymera_udp_transport_s {
    qymera_udp_socket_t discovery_sock;
    qymera_udp_socket_t control_sock;
    qymera_ring_t rx_ring;
    qymera_ring_t tx_ring;
    uint32_t local_uid;
    uint32_t tx_seq;
    
    qymera_udp_msg_cb_t callbacks[16];
    void *callback_contexts[16];
};

/* Internal message event structure for ring buffer */
typedef struct {
    qymera_msg_header_t header;
    char src_ip[16];
    uint16_t src_port;
    uint8_t payload[QYMERA_MAX_UDP_PACKET - sizeof(qymera_msg_header_t)];
} qymera_udp_msg_event_t;

static qymera_err_t udp_parse_and_enqueue(qymera_udp_transport_t *t, qymera_udp_socket_t sock) {
    struct qymera_udp_socket *s = (struct qymera_udp_socket *)sock;
    uint8_t buffer[QYMERA_MAX_UDP_PACKET];
    struct sockaddr_in src = {0};
    socklen_t src_len = sizeof(struct sockaddr_in);
    
    ssize_t recv_len = recvfrom(s->sockfd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&src, &src_len);
    
    if (recv_len < 0) {
        return QYMERA_ERR_TIMEOUT;
    }
    
    if (recv_len < (ssize_t)sizeof(qymera_msg_header_t)) {
        return QYMERA_ERR_PROTOCOL;
    }
    
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    
    if (header->magic != QYMERA_PROTOCOL_MAGIC) return QYMERA_ERR_PROTOCOL;
    if (header->version != QYMERA_PROTOCOL_VERSION) return QYMERA_ERR_PROTOCOL;
    if (header->len != (uint16_t)(recv_len - sizeof(qymera_msg_header_t))) return QYMERA_ERR_PROTOCOL;
    if (header->kind == 0 || header->kind >= 16) return QYMERA_ERR_PROTOCOL;
    
    qymera_udp_msg_event_t msg_event;
    memcpy(&msg_event.header, header, sizeof(qymera_msg_header_t));
    inet_ntop(AF_INET, &src.sin_addr, msg_event.src_ip, sizeof(msg_event.src_ip));
    msg_event.src_port = ntohs(src.sin_port);
    memcpy(msg_event.payload, buffer + sizeof(qymera_msg_header_t), header->len);
    
    return qymera_ring_push(&t->rx_ring, &msg_event);
}

qymera_err_t qymera_udp_transport_init(qymera_udp_transport_t **transport, const qymera_udp_transport_config_t *config) {
    if (!transport || !config) return QYMERA_ERR_INVALID_ARG;
    if (!config->discovery_sock || !config->control_sock) return QYMERA_ERR_INVALID_ARG;
    if (!config->rx_ring.data || !config->tx_ring.data) return QYMERA_ERR_INVALID_ARG;
    
    qymera_udp_transport_t *tr = calloc(1, sizeof(qymera_udp_transport_t));
    if (!tr) return QYMERA_ERR_NO_SPACE;
    
    tr->discovery_sock = config->discovery_sock;
    tr->control_sock = config->control_sock;
    tr->rx_ring = config->rx_ring;
    tr->tx_ring = config->tx_ring;
    tr->local_uid = config->local_uid;
    tr->tx_seq = config->tx_seq ? config->tx_seq : 1;
    
    *transport = tr;
    return QYMERA_OK;
}

size_t qymera_udp_transport_receive(qymera_udp_transport_t *transport) {
    if (!transport) return 0;
    
    size_t processed = 0;
    
    // Drain discovery socket
    for (int i = 0; i < 8; i++) {
        qymera_err_t err = udp_parse_and_enqueue(transport, transport->discovery_sock);
        if (err == QYMERA_ERR_TIMEOUT) break;
        if (err == QYMERA_OK) processed++;
    }
    
    // Drain control socket
    for (int i = 0; i < 8; i++) {
        qymera_err_t err = udp_parse_and_enqueue(transport, transport->control_sock);
        if (err == QYMERA_ERR_TIMEOUT) break;
        if (err == QYMERA_OK) processed++;
    }
    
    // Process received messages from ring buffer
    qymera_udp_msg_event_t msg_event;
    
    while (qymera_ring_pop(&transport->rx_ring, &msg_event) == QYMERA_OK) {
        uint8_t kind = msg_event.header.kind;
        if (kind < 16 && transport->callbacks[kind]) {
            transport->callbacks[kind](transport, &msg_event.header,
                                       msg_event.payload, msg_event.header.len,
                                       msg_event.src_ip, msg_event.src_port);
        }
        processed++;
    }
    
    return processed;
}

qymera_err_t qymera_udp_transport_send(qymera_udp_transport_t *transport, qymera_msg_type_t msg_type,
                                        const char *dest_ip, uint16_t dest_port,
                                        const void *payload, size_t payload_len) {
    if (!transport || !dest_ip || (!payload && payload_len > 0)) return QYMERA_ERR_INVALID_ARG;
    if (payload_len > QYMERA_MAX_UDP_PACKET - sizeof(qymera_msg_header_t)) return QYMERA_ERR_INVALID_ARG;
    
    uint8_t buffer[QYMERA_MAX_UDP_PACKET];
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = msg_type;
    header->seq = transport->tx_seq++;
    header->src_uid = transport->local_uid;
    header->len = (uint16_t)payload_len;
    
    if (payload && payload_len > 0) {
        memcpy(buffer + sizeof(qymera_msg_header_t), payload, payload_len);
    }
    
    qymera_udp_socket_t sock = (msg_type == QYMERA_MSG_DISCOVER || msg_type == QYMERA_MSG_ANNOUNCE) 
                               ? transport->discovery_sock : transport->control_sock;
    
    return qymera_udp_socket_send(sock, dest_ip, dest_port, buffer, sizeof(qymera_msg_header_t) + payload_len);
}

qymera_err_t qymera_udp_transport_send_command(qymera_udp_transport_t *transport, const char *dest_ip,
                                                uint32_t entity_id, uint8_t opcode,
                                                float value_f, uint32_t value_u32,
                                                uint32_t *cmd_seq) {
    if (!transport || !dest_ip || !cmd_seq) return QYMERA_ERR_INVALID_ARG;
    
    /* Allocate a single correlation ID for this command. It is used both as the
     * message header.seq and as the payload cmd_seq so the remote peer has one
     * unambiguous identifier to echo back in the ACK. */
    uint32_t id = transport->tx_seq++;
    
    uint8_t buffer[QYMERA_MAX_UDP_PACKET];
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_COMMAND;
    header->seq = id;
    header->src_uid = transport->local_uid;
    header->len = (uint16_t)sizeof(qymera_payload_command_t);
    
    qymera_payload_command_t *cmd = (qymera_payload_command_t *)(buffer + sizeof(qymera_msg_header_t));
    cmd->entity_id = entity_id;
    cmd->opcode = opcode;
    cmd->value_f = value_f;
    cmd->value_u32 = value_u32;
    cmd->cmd_seq = id;
    *cmd_seq = id;
    
    return qymera_udp_socket_send(transport->control_sock, dest_ip, QYMERA_UDP_PORT_CONTROL,
                                  buffer, sizeof(qymera_msg_header_t) + sizeof(qymera_payload_command_t));
}

void qymera_udp_transport_set_callback(qymera_udp_transport_t *transport, qymera_msg_type_t msg_type,
                                        qymera_udp_msg_cb_t callback, void *context) {
    if (!transport || msg_type >= 16) return;
    transport->callbacks[msg_type] = callback;
    transport->callback_contexts[msg_type] = context;
}

void qymera_udp_transport_get_stats(qymera_udp_transport_t *transport, qymera_ring_stats_t *rx_stats, qymera_ring_stats_t *tx_stats) {
    if (!transport) return;
    qymera_ring_get_stats(&transport->rx_ring, rx_stats);
    qymera_ring_get_stats(&transport->tx_ring, tx_stats);
}

/* =========================
 * Helper functions for message parsing
 * ========================= */

qymera_err_t qymera_udp_parse_register(const uint8_t *payload, size_t len, qymera_payload_register_t *out) {
    if (!payload || !out || len != sizeof(qymera_payload_register_t)) return QYMERA_ERR_PROTOCOL;
    memcpy(out, payload, sizeof(qymera_payload_register_t));
    return QYMERA_OK;
}

qymera_err_t qymera_udp_parse_entity_sample(const uint8_t *payload, size_t len, qymera_payload_entity_sample_t *out) {
    if (!payload || !out || len != sizeof(qymera_payload_entity_sample_t)) return QYMERA_ERR_PROTOCOL;
    memcpy(out, payload, sizeof(qymera_payload_entity_sample_t));
    return QYMERA_OK;
}

qymera_err_t qymera_udp_parse_command(const uint8_t *payload, size_t len, qymera_payload_command_t *out) {
    if (!payload || !out || len != sizeof(qymera_payload_command_t)) return QYMERA_ERR_PROTOCOL;
    memcpy(out, payload, sizeof(qymera_payload_command_t));
    return QYMERA_OK;
}

qymera_err_t qymera_udp_parse_ack(const uint8_t *payload, size_t len, qymera_payload_ack_t *out) {
    if (!payload || !out || len != sizeof(qymera_payload_ack_t)) return QYMERA_ERR_PROTOCOL;
    memcpy(out, payload, sizeof(qymera_payload_ack_t));
    return QYMERA_OK;
}

/* =========================
 * Helper functions for building messages
 * ========================= */

void qymera_udp_build_discover(uint8_t *buffer, size_t *len, uint32_t local_uid) {
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_DISCOVER;
    header->seq = 0; // Will be set by transport
    header->src_uid = local_uid;
    header->len = 0;
    *len = sizeof(qymera_msg_header_t);
}

void qymera_udp_build_announce(uint8_t *buffer, size_t *len, uint32_t local_uid, 
                                uint32_t entity_id, uint8_t type, uint8_t flags,
                                float value_f, uint32_t value_u32, uint32_t src_ts, int8_t rssi) {
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_ANNOUNCE;
    header->seq = 0; // Will be set by transport
    header->src_uid = local_uid;
    header->len = sizeof(qymera_payload_entity_sample_t);
    
    qymera_payload_entity_sample_t *payload = (qymera_payload_entity_sample_t *)(buffer + sizeof(qymera_msg_header_t));
    payload->entity_id = entity_id;
    payload->type = type;
    payload->flags = flags;
    payload->value_f = value_f;
    payload->value_u32 = value_u32;
    payload->src_ts = src_ts;
    payload->rssi = rssi;
    
    *len = sizeof(qymera_msg_header_t) + sizeof(qymera_payload_entity_sample_t);
}

void qymera_udp_build_command(uint8_t *buffer, size_t *len, uint32_t local_uid,
                               uint32_t entity_id, uint8_t opcode, uint8_t flags,
                               float value_f, uint32_t value_u32, uint32_t cmd_seq) {
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_COMMAND;
    header->seq = 0; // Will be set by transport
    header->src_uid = local_uid;
    header->len = sizeof(qymera_payload_command_t);
    
    qymera_payload_command_t *payload = (qymera_payload_command_t *)(buffer + sizeof(qymera_msg_header_t));
    payload->entity_id = entity_id;
    payload->opcode = opcode;
    payload->flags = flags;
    payload->value_f = value_f;
    payload->value_u32 = value_u32;
    payload->cmd_seq = cmd_seq;
    
    *len = sizeof(qymera_msg_header_t) + sizeof(qymera_payload_command_t);
}

void qymera_udp_build_ack(uint8_t *buffer, size_t *len, uint32_t local_uid, uint32_t cmd_seq, uint8_t result) {
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_ACK;
    header->seq = 0; // Will be set by transport
    header->src_uid = local_uid;
    header->len = sizeof(qymera_payload_ack_t);
    
    qymera_payload_ack_t *payload = (qymera_payload_ack_t *)(buffer + sizeof(qymera_msg_header_t));
    payload->cmd_seq = cmd_seq;
    payload->result = result;
    
    *len = sizeof(qymera_msg_header_t) + sizeof(qymera_payload_ack_t);
}

void qymera_udp_build_register(uint8_t *buffer, size_t *len, uint32_t local_uid,
                                uint32_t chip_uid, const char *model, const char *fw_version,
                                uint32_t capability_mask, uint32_t entity_roster_hash) {
    qymera_msg_header_t *header = (qymera_msg_header_t *)buffer;
    header->magic = QYMERA_PROTOCOL_MAGIC;
    header->version = QYMERA_PROTOCOL_VERSION;
    header->kind = QYMERA_MSG_REGISTER;
    header->seq = 0; // Will be set by transport
    header->src_uid = local_uid;
    header->len = sizeof(qymera_payload_register_t);
    
    qymera_payload_register_t *payload = (qymera_payload_register_t *)(buffer + sizeof(qymera_msg_header_t));
    payload->chip_uid = chip_uid;
    strncpy(payload->model, model, sizeof(payload->model) - 1);
    strncpy(payload->fw_version, fw_version, sizeof(payload->fw_version) - 1);
    payload->capability_mask = capability_mask;
    payload->entity_roster_hash = entity_roster_hash;
    
    *len = sizeof(qymera_msg_header_t) + sizeof(qymera_payload_register_t);
}