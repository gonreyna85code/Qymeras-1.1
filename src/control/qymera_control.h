/**
 * Qymera Dashboard - Control Context + Remote Command Machine
 *
 * Owns the strongly-typed control context used by the Control API, and the
 * bounded pending-command state machine that tracks remote relay/dimmer
 * commands from dispatch through ACK/state confirmation (or timeout/failure).
 *
 * The Rule Engine stays transport-agnostic: it calls the Control API and never
 * sees the UDP transport or the pending-command table.
 */
#pragma once

#include "qymera_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-size pending-command table. No heap allocation per command. */
#define QYMERA_MAX_PENDING_COMMANDS 8
#define QYMERA_COMMAND_TIMEOUT_MS   2000

/* Command lifecycle status (sequential for the happy path) */
typedef enum {
    QYMERA_CMD_REQUESTED = 0, /* Command created, not yet dispatched */
    QYMERA_CMD_DISPATCHED,    /* Control accepted, not yet sent/acked */
    QYMERA_CMD_WAITING_ACK,   /* Sent over UDP, awaiting remote ACK */
    QYMERA_CMD_ACKED,         /* Remote ACKed (accepted), awaiting authoritative state */
    QYMERA_CMD_STATE_CONFIRMED, /* Remote reported matching state: confirmed */
    QYMERA_CMD_FAILED,        /* Remote error / invalid response / dispatch failed */
    QYMERA_CMD_TIMEOUT,       /* No ACK/state within deadline */
} qymera_cmd_status_t;

/* Reliability semantics for the registry value (replaces magic integers) */
typedef enum {
    QYMERA_RELIABILITY_UNKNOWN = 0,
    QYMERA_RELIABILITY_PENDING,   /* Remote command dispatched, not confirmed */
    QYMERA_RELIABILITY_CONFIRMED, /* Matches authoritative remote state */
    QYMERA_RELIABILITY_STALE,     /* No recent report */
    QYMERA_RELIABILITY_FAILED,    /* Command failed / timed out */
} qymera_reliability_t;

typedef struct {
    bool used;
    uint32_t cmd_seq;                 /* Correlation ID (header.seq == cmd_seq) */
    char dest_ip[16];                 /* Where the command was sent */
    char device_id[QYMERA_DEVICE_ID_LEN];
    char entity_id[QYMERA_ENTITY_ID_LEN];
    uint8_t opcode;                   /* 1=relay, 2=dimmer */
    float requested_value;            /* 1.0/0.0 relay, 0-100 dimmer */
    bool desired_bool;
    float desired_numeric;
    uint32_t started_at;              /* uptime ms */
    uint32_t deadline;                /* started_at + timeout */
    qymera_cmd_status_t status;
} qymera_pending_command_t;

/* Forward declarations for pointers only */
typedef struct qymera_registry_s qymera_registry_t;
typedef struct qymera_udp_transport_s qymera_udp_transport_t;
typedef struct qymera_event_bus_s qymera_event_bus_t;
typedef struct qymera_log_s qymera_log_t;

/* The strongly-typed control context. The Control API never reinterprets a
 * qymera_core_t* as a transport: it is given this typed struct. */
typedef struct {
    qymera_registry_t *registry;
    qymera_udp_transport_t *udp;
    qymera_event_bus_t *event_bus;
    qymera_log_t *log;
    uint32_t cmd_seq;                 /* Next command correlation ID */
    qymera_pending_command_t pending[QYMERA_MAX_PENDING_COMMANDS];
} qymera_control_context_t;

/* Lifecycle */
qymera_err_t qymera_control_context_init(qymera_control_context_t *context,
                                          qymera_registry_t *registry,
                                          qymera_udp_transport_t *udp,
                                          qymera_event_bus_t *event_bus,
                                          qymera_log_t *log);
void qymera_control_context_cleanup(qymera_control_context_t *context);

/* Control API (transport-agnostic entrypoints used by the Rule Engine) */
qymera_err_t qymera_control_set_relay(qymera_control_context_t *context,
                                      const qymera_entity_ref_t *entity_ref,
                                      bool state, bool local_only);
qymera_err_t qymera_control_set_dimmer(qymera_control_context_t *context,
                                       const qymera_entity_ref_t *entity_ref,
                                       uint8_t level, bool local_only);

/* Runtime message handling (called from UDP transport receive path) */
void qymera_control_on_ack(qymera_control_context_t *context,
                           uint32_t ack_cmd_seq, uint8_t ack_result,
                           const char *src_ip);
void qymera_control_on_state(qymera_control_context_t *context,
                             uint32_t entity_id, uint8_t entity_type,
                             const float *value_f, const uint32_t *value_u32,
                             bool has_float, bool has_u32,
                             const char *src_ip);

/* Periodic processing: resolve timeouts without blocking the main loop */
void qymera_control_tick(qymera_control_context_t *context, uint32_t now_ms);

/* Introspection */
uint32_t qymera_control_pending_count(const qymera_control_context_t *context);
qymera_cmd_status_t qymera_control_pending_status(const qymera_control_context_t *context,
                                                   uint32_t cmd_seq);

#ifdef __cplusplus
}
#endif
