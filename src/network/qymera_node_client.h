/**
 * Qymera Dashboard - Node v1 HTTP Client
 *
 * The single boundary between the Dashboard and remote Qymera Nodes under the
 * v1 integration contract (see docs/api-contract.yaml). The Dashboard NEVER
 * speaks raw UDP/ESP-NOW to Nodes; all Node discovery, state and command
 * traffic goes through this application-layer HTTP client.
 *
 * Responsibilities:
 *   - Configured node targets (host:port), persisted as a small NVS blob.
 *   - Periodic reconcile: GET /api/v1/status + GET /api/v1/entities per
 *     target; upserts devices/entities into the registry; feeds authoritative
 *     entity state + device online/offline transitions to callbacks.
 *   - Commands: POST /api/v1/entities/<id>/command -> accepted & status/error.
 *   - Version negotiation data (api_version / protocol_version / firmware
 *     version / capabilities) is read from /status and stored on the device so
 *     the rest of the system can surface incompatible Nodes.
 *
 * The HTTP surface (paths, schema, error codes) implements the working draft
 * in docs/api-contract.yaml and is validated by tests/integration against the
 * mock Node. When the authoritative firmware v1 API lands, reconcile only
 * inside this module (and the mock/tests it drives).
 */
#pragma once

#include "qymera_types.h"
#include "qymera_registry.h"
#include "qymera_log.h"
#include "qymera_storage.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Node discovery/control surface limits */
#define QYMERA_MAX_NODE_TARGETS 4
#define QYMERA_NODE_HOST_LEN    64
#define QYMERA_NODE_HTTP_BUF    4096  /* max heap buffer per HTTP response   */
#define QYMERA_NODE_POLL_MS     5000  /* default reconcile cadence per node  */
#define QYMERA_NODE_TIMEOUT_MS  3000  /* connect+recv total budget per call  */

typedef struct {
    char host[QYMERA_NODE_HOST_LEN];
    uint16_t port;
    uint16_t poll_interval_ms;   /* 0 -> QYMERA_NODE_POLL_MS */
} qymera_node_target_t;

typedef struct {
    uint8_t count;
    qymera_node_target_t targets[QYMERA_MAX_NODE_TARGETS];
} qymera_node_target_set_t;

typedef struct qymera_node_client_s qymera_node_client_t;

/* Authoritative entity state consumed from a node snapshot.
 * bool_value is meaningful for relay/digital entities; numeric_value for
 * numeric sensors/actuators. type is a qymera_entity_type_t value. */
typedef void (*qymera_node_entity_cb_t)(void *ctx,
                                        const char *device_id,
                                        const char *entity_id,
                                        bool available, uint8_t type,
                                        float numeric_value, bool bool_value);

typedef void (*qymera_node_online_cb_t)(void *ctx, const char *device_id, bool online);

typedef struct {
    qymera_registry_t *registry;
    qymera_log_t *log;
    qymera_storage_t *storage;
    qymera_node_entity_cb_t on_entity_state;
    qymera_node_online_cb_t on_device_online;
    void *callback_ctx;
} qymera_node_client_config_t;

qymera_err_t qymera_node_client_init(qymera_node_client_t **client,
                                     const qymera_node_client_config_t *config);
void qymera_node_client_cleanup(qymera_node_client_t *client);

/* Configured targets: in-memory + persisted (NVS blob "nodes"). */
qymera_err_t qymera_node_client_set_targets(qymera_node_client_t *client,
                                            const qymera_node_target_set_t *set);
qymera_err_t qymera_node_client_load_targets(qymera_node_client_t *client);
qymera_err_t qymera_node_client_get_targets(const qymera_node_client_t *client,
                                            qymera_node_target_set_t *out);

/* Periodic reconcile + offline marking. Call from core tick. */
void qymera_node_client_tick(qymera_node_client_t *client);

/* Dispatch one command to a node. host/port identify the node (the target
 * that owns the entity). `accepted` mirrors the node's response; on a node
 * error the canonical code is copied into err_code (when provided). */
qymera_err_t qymera_node_client_send_command(qymera_node_client_t *client,
                                             const char *host, uint16_t port,
                                             const char *device_id,
                                             const char *entity_id,
                                             uint8_t opcode, float value_f,
                                             bool *accepted,
                                             char *err_code, size_t err_sz);

/* Diagnostics for the HTTP API / UI. */
void qymera_node_client_stats(const qymera_node_client_t *client,
                              uint32_t *polls_ok, uint32_t *polls_fail,
                              uint32_t *commands_sent, uint32_t *commands_err);

#ifdef __cplusplus
}
#endif