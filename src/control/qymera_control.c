/**
 * Qymera Dashboard - Control Context + Remote Command State Machine
 *
 * Implementation of the typed control context and the bounded pending-command
 * table that tracks remote relay/dimmer commands from dispatch through node
 * acceptance (HTTP command result) to authoritative snapshot-state
 * confirmation (or timeout/failure).
 */
#include "qymera_control.h"
#include "qymera_registry.h"
#include "qymera_node_client.h"
#include "qymera_hal.h"
#include "qymera_event_bus.h"
#include "qymera_log.h"
#include <string.h>
#include <stdio.h>

/* ---- Entity capability check -------------------------------------------- */

static bool control_entity_has_capability(qymera_control_context_t *context,
                                          const qymera_entity_ref_t *entity_ref,
                                          qymera_capability_t cap,
                                          uint16_t *entity_idx_out) {
    qymera_registry_t *reg = context->registry;
    uint16_t entity_idx;
    if (qymera_registry_find_entity(reg, entity_ref->device_id, entity_ref->entity_id, &entity_idx) != QYMERA_OK) {
        return false;
    }
    qymera_entity_t entity;
    if (qymera_registry_get_entity(reg, entity_idx, &entity) != QYMERA_OK) {
        return false;
    }
    for (uint8_t i = 0; i < entity.capability_count; i++) {
        if (entity.capabilities[i] == cap) {
            if (entity_idx_out) *entity_idx_out = entity_idx;
            return true;
        }
    }
    return false;
}

/* Map the node error code string to a qymera_err_t (surface for callers).
 * The wire carries HTTP status only; qymera_node_client maps status to these
 * stable codes. */
static qymera_err_t node_error_to_err(const char *code) {
    if (!code || !code[0]) return QYMERA_ERR_PROTOCOL;
    if (strcmp(code, "DEVICE_OFFLINE") == 0) return QYMERA_ERR_NETWORK;
    if (strcmp(code, "ENTITY_NOT_FOUND") == 0) return QYMERA_ERR_NOT_FOUND;
    if (strcmp(code, "DEVICE_NOT_FOUND") == 0) return QYMERA_ERR_NOT_FOUND;
    if (strcmp(code, "COMMAND_NOT_SUPPORTED") == 0) return QYMERA_ERR_INVALID_CAPABILITY;
    if (strcmp(code, "INVALID_VALUE") == 0 || strcmp(code, "INVALID_INPUT") == 0) return QYMERA_ERR_INVALID_ARG;
    if (strcmp(code, "NOT_AUTHORIZED") == 0) return QYMERA_ERR_INVALID_STATE;
    if (strcmp(code, "RATE_LIMITED") == 0) return QYMERA_ERR_BUSY;
    if (strcmp(code, "TIMEOUT") == 0) return QYMERA_ERR_TIMEOUT;
    if (strcmp(code, "METHOD_NOT_ALLOWED") == 0) return QYMERA_ERR_PROTOCOL;
    return QYMERA_ERR_PROTOCOL;
}

/* ---- Control context lifecycle ------------------------------------------ */

qymera_err_t qymera_control_context_init(qymera_control_context_t *context,
                                          qymera_registry_t *registry,
                                          qymera_node_client_t *node_client,
                                          qymera_event_bus_t *event_bus,
                                          qymera_log_t *log) {
    if (!context) return QYMERA_ERR_INVALID_ARG;
    memset(context, 0, sizeof(*context));
    context->registry = registry;
    context->node_client = node_client;
    context->event_bus = event_bus;
    context->log = log;
    context->cmd_seq = 1;
    return QYMERA_OK;
}

void qymera_control_context_cleanup(qymera_control_context_t *context) {
    if (!context) return;
    memset(context, 0, sizeof(*context));
}

uint32_t qymera_control_pending_count(const qymera_control_context_t *context) {
    if (!context) return 0;
    uint32_t count = 0;
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        if (context->pending[i].used) count++;
    }
    return count;
}

qymera_cmd_status_t qymera_control_pending_status(const qymera_control_context_t *context,
                                                   uint32_t cmd_seq) {
    if (!context) return QYMERA_CMD_TIMEOUT;
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        if (context->pending[i].used && context->pending[i].cmd_seq == cmd_seq) {
            return context->pending[i].status;
        }
    }
    return QYMERA_CMD_TIMEOUT; /* not found -> already resolved */
}

/* ---- Pending table helpers ---------------------------------------------- */

static qymera_pending_command_t *pending_alloc(qymera_control_context_t *context) {
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        if (!context->pending[i].used) {
            return &context->pending[i];
        }
    }
    return NULL;
}

static void pending_terminal(qymera_pending_command_t *entry, qymera_cmd_status_t status) {
    entry->status = status;
    entry->used = false; /* terminal state frees the slot */
}

/* ---- Control API: relay -------------------------------------------------- */

qymera_err_t qymera_control_set_relay(qymera_control_context_t *context,
                                      const qymera_entity_ref_t *entity_ref,
                                      bool state, bool local_only) {
    if (!context || !entity_ref || !context->registry) return QYMERA_ERR_INVALID_ARG;

    uint16_t entity_idx;
    if (!control_entity_has_capability(context, entity_ref, QYMERA_CAP_ACTUATOR_RELAY, &entity_idx)) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    qymera_entity_t entity;
    if (qymera_registry_get_entity(context->registry, entity_idx, &entity) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }

    /* Determine device locality */
    uint16_t dev_idx;
    if (qymera_registry_find_device(context->registry, entity_ref->device_id, &dev_idx) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }
    qymera_device_t device;
    if (qymera_registry_get_device(context->registry, dev_idx, &device) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }
    bool is_remote = (device.role == 1);

    if (local_only && is_remote) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    uint32_t now_ms = qymera_system_get_uptime_ms();

    /* Desired state model (shared for local and remote) */
    qymera_entity_value_t desired = {0};
    desired.valid = true;
    desired.bool_value = state;
    desired.numeric_value = state ? 1.0f : 0.0f;
    desired.timestamp = qymera_timestamp_now();

    if (!is_remote) {
        /* Local actuator: synchronous/confirmed on hardware success */
        if (entity.value.bool_value != state) {
            qymera_err_t gpio_err = qymera_gpio_write(entity.gpio_pin,
                        state ? QYMERA_GPIO_HIGH : QYMERA_GPIO_LOW);
            if (gpio_err != QYMERA_OK) {
                return gpio_err;
            }
        }
        qymera_entity_value_t observed = {0};
        observed.valid = true;
        observed.bool_value = state;
        observed.numeric_value = state ? 1.0f : 0.0f;
        observed.timestamp = qymera_timestamp_now();
        observed.reliability = QYMERA_RELIABILITY_CONFIRMED;
        qymera_registry_update_entity_value(context->registry, entity_idx, &observed);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_STATE_CONFIRMED);

        if (context->event_bus) {
            qymera_event_t ev;
            qymera_event_make_actuator_changed(&ev, entity_ref->device_id, entity_ref->entity_id, &observed);
            qymera_event_bus_publish(context->event_bus, &ev);
        }
        if (context->log) qymera_log_action(context->log, "control", "relay %s -> %s (local)", entity_ref->entity_id, state ? "ON" : "OFF");
        return QYMERA_OK;
    }

    /* Remote device: authoritative snapshot already at the requested state ->
     * nothing to dispatch (the Node's /toggle flips state; toggling when the
     * physical state already matches would invert it). */
    if (entity.value.valid && entity.value.bool_value == state) {
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_STATE_CONFIRMED);
        if (context->log) qymera_log_action(context->log, "control", "relay %s already %s (remote %s)",
                                            entity_ref->entity_id, state ? "ON" : "OFF", device.ip_addr);
        return QYMERA_OK;
    }

    /* Remote device: bounded pending dispatch through the Node HTTP API. */
    if (!context->node_client) return QYMERA_ERR_NETWORK;

    qymera_pending_command_t *entry = pending_alloc(context);
    if (!entry) return QYMERA_ERR_NO_SPACE; /* pending table full */

    memset(entry, 0, sizeof(*entry));
    strncpy(entry->device_id, entity_ref->device_id, QYMERA_DEVICE_ID_LEN - 1);
    strncpy(entry->entity_id, entity_ref->entity_id, QYMERA_ENTITY_ID_LEN - 1);
    strncpy(entry->dest_ip, device.ip_addr, sizeof(entry->dest_ip) - 1);
    entry->opcode = 1;
    entry->requested_value = state ? 1.0f : 0.0f;
    entry->desired_bool = state;
    entry->desired_numeric = entry->requested_value;
    entry->status = QYMERA_CMD_DISPATCHED;
    entry->started_at = now_ms;
    entry->deadline = now_ms + QYMERA_COMMAND_TIMEOUT_MS;
    entry->cmd_seq = context->cmd_seq++;
    entry->used = true;

    bool accepted = false;
    char err_code[24] = {0};
    qymera_err_t send_err = qymera_node_client_send_command(
        context->node_client, device.ip_addr, device.port,
        entity_ref->device_id, entity_ref->entity_id,
        1, entry->requested_value, &accepted, err_code, sizeof(err_code));
    if (send_err != QYMERA_OK) {
        /* Transport failure -> terminal FAILED, no zombie entry */
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        if (context->log) qymera_log_warn(context->log, "control", "relay %s dispatch failed (%d)",
                                          entity_ref->entity_id, (int)send_err);
        return send_err;
    }

    if (!accepted) {
        /* Node rejected the command; canonical error code available. */
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        if (context->log) qymera_log_warn(context->log, "control", "relay %s rejected by node (%s)",
                                          entity_ref->entity_id, err_code[0] ? err_code : "REJECTED");
        return node_error_to_err(err_code);
    }

    entry->status = QYMERA_CMD_ACKED; /* node accepted; awaiting authoritative state */

    desired.reliability = QYMERA_RELIABILITY_PENDING;
    qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_ACKED);

    if (context->log) qymera_log_action(context->log, "control", "relay %s -> %s accepted seq=%lu (remote %s)",
                                        entity_ref->entity_id, state ? "ON" : "OFF",
                                        (unsigned long)entry->cmd_seq, device.ip_addr);

    /* Dispatched async: QYMERA_OK means accepted/dispatched, not confirmed */
    return QYMERA_OK;
}

/* ---- Control API: dimmer ------------------------------------------------- */

qymera_err_t qymera_control_set_dimmer(qymera_control_context_t *context,
                                       const qymera_entity_ref_t *entity_ref,
                                       uint8_t level, bool local_only) {
    if (!context || !entity_ref || !context->registry) return QYMERA_ERR_INVALID_ARG;
    if (level > 100) return QYMERA_ERR_INVALID_ARG;

    uint16_t entity_idx;
    if (!control_entity_has_capability(context, entity_ref, QYMERA_CAP_ACTUATOR_DIMMER, &entity_idx)) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    qymera_entity_t entity;
    if (qymera_registry_get_entity(context->registry, entity_idx, &entity) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }

    uint16_t dev_idx;
    if (qymera_registry_find_device(context->registry, entity_ref->device_id, &dev_idx) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }
    qymera_device_t device;
    if (qymera_registry_get_device(context->registry, dev_idx, &device) != QYMERA_OK) {
        return QYMERA_ERR_NOT_FOUND;
    }
    bool is_remote = (device.role == 1);

    if (local_only && is_remote) {
        return QYMERA_ERR_INVALID_CAPABILITY;
    }

    uint32_t now_ms = qymera_system_get_uptime_ms();

    qymera_entity_value_t desired = {0};
    desired.valid = true;
    desired.numeric_value = (float)level;
    desired.bool_value = false;
    desired.timestamp = qymera_timestamp_now();

    if (!is_remote) {
        if (entity.value.numeric_value != (float)level) {
            if (entity.gpio_pin < 0) return QYMERA_ERR_INVALID_STATE;
            qymera_err_t pwm_err = qymera_pwm_set_duty(entity.gpio_pin, level);
            if (pwm_err != QYMERA_OK) return pwm_err;
        }
        qymera_entity_value_t observed = {0};
        observed.valid = true;
        observed.numeric_value = (float)level;
        observed.timestamp = qymera_timestamp_now();
        observed.reliability = QYMERA_RELIABILITY_CONFIRMED;
        qymera_registry_update_entity_value(context->registry, entity_idx, &observed);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_STATE_CONFIRMED);

        if (context->event_bus) {
            qymera_event_t ev;
            qymera_event_make_actuator_changed(&ev, entity_ref->device_id, entity_ref->entity_id, &observed);
            qymera_event_bus_publish(context->event_bus, &ev);
        }
        if (context->log) qymera_log_action(context->log, "control", "dimmer %s -> %u (local)", entity_ref->entity_id, level);
        return QYMERA_OK;
    }

    /* Remote device: snapshot already at the requested level -> no dispatch. */
    if (entity.value.valid && (int)entity.value.numeric_value == level) {
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_STATE_CONFIRMED);
        if (context->log) qymera_log_action(context->log, "control", "dimmer %s already at %u (remote %s)",
                                            entity_ref->entity_id, level, device.ip_addr);
        return QYMERA_OK;
    }

    if (!context->node_client) return QYMERA_ERR_NETWORK;

    qymera_pending_command_t *entry = pending_alloc(context);
    if (!entry) return QYMERA_ERR_NO_SPACE;

    memset(entry, 0, sizeof(*entry));
    strncpy(entry->device_id, entity_ref->device_id, QYMERA_DEVICE_ID_LEN - 1);
    strncpy(entry->entity_id, entity_ref->entity_id, QYMERA_ENTITY_ID_LEN - 1);
    strncpy(entry->dest_ip, device.ip_addr, sizeof(entry->dest_ip) - 1);
    entry->opcode = 2;
    entry->requested_value = (float)level;
    entry->desired_bool = false;
    entry->desired_numeric = (float)level;
    entry->status = QYMERA_CMD_DISPATCHED;
    entry->started_at = now_ms;
    entry->deadline = now_ms + QYMERA_COMMAND_TIMEOUT_MS;
    entry->cmd_seq = context->cmd_seq++;
    entry->used = true;

    bool accepted = false;
    char err_code[24] = {0};
    qymera_err_t send_err = qymera_node_client_send_command(
        context->node_client, device.ip_addr, device.port,
        entity_ref->device_id, entity_ref->entity_id,
        2, (float)level, &accepted, err_code, sizeof(err_code));
    if (send_err != QYMERA_OK) {
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        if (context->log) qymera_log_warn(context->log, "control", "dimmer %s dispatch failed (%d)",
                                          entity_ref->entity_id, (int)send_err);
        return send_err;
    }

    if (!accepted) {
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        if (context->log) qymera_log_warn(context->log, "control", "dimmer %s rejected by node (%s)",
                                          entity_ref->entity_id, err_code[0] ? err_code : "REJECTED");
        return node_error_to_err(err_code);
    }

    entry->status = QYMERA_CMD_ACKED;

    desired.reliability = QYMERA_RELIABILITY_PENDING;
    qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_ACKED);

    if (context->log) qymera_log_action(context->log, "control", "dimmer %s -> %u accepted seq=%lu (remote %s)",
                                        entity_ref->entity_id, level,
                                        (unsigned long)entry->cmd_seq, device.ip_addr);
    return QYMERA_OK;
}

/* ---- Remote authoritative state handling ----------------------------------
 * Called from the node reconciliation path (qymera_node_client) for every
 * entity in the snapshot. The client has already ingested the observed value
 * into the registry; this function resolves pending commands whose requested
 * state matches the authoritative value. HTTP 200 / command acceptance does
 * NOT confirm an actuator reached its state: only the Node snapshot does. */
void qymera_control_on_remote_state(qymera_control_context_t *context,
                                    const char *device_id, const char *entity_id,
                                    bool available, uint8_t entity_type,
                                    float numeric_value, bool bool_value) {
    (void)entity_type;
    if (!context || !device_id || !entity_id) return;
    /* Not available (stale/offline): cannot confirm; keep pending. */
    if (!available) return;

    uint16_t entity_idx;
    if (qymera_registry_find_entity(context->registry, device_id, entity_id, &entity_idx) != QYMERA_OK) {
        return;
    }
    qymera_entity_t e;
    if (qymera_registry_get_entity(context->registry, entity_idx, &e) != QYMERA_OK) {
        return;
    }

    bool resolved = false;
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        qymera_pending_command_t *entry = &context->pending[i];
        if (!entry->used) continue;
        if (strcmp(entry->device_id, device_id) != 0) continue;
        if (strcmp(entry->entity_id, entity_id) != 0) continue;

        bool matched;
        if (entry->opcode == 1) {
            matched = (entry->desired_bool == bool_value);
        } else {
            matched = (entry->desired_numeric == numeric_value) ||
                      (entry->desired_numeric - numeric_value > -0.5f &&
                       entry->desired_numeric - numeric_value < 0.5f);
        }

        if (matched) {
            qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_STATE_CONFIRMED);
            if (context->log) qymera_log_event(context->log, "control", "state %s/%s CONFIRMED seq=%lu",
                                               device_id, entity_id, (unsigned long)entry->cmd_seq);
        } else {
            qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_FAILED);
            if (context->log) qymera_log_warn(context->log, "control", "state %s/%s MISMATCH -> FAILED seq=%lu",
                                              device_id, entity_id, (unsigned long)entry->cmd_seq);
        }
        pending_terminal(entry, matched ? QYMERA_CMD_STATE_CONFIRMED : QYMERA_CMD_FAILED);
        resolved = true;
    }

    if (resolved && context->event_bus) {
        qymera_event_t ev;
        qymera_entity_value_t observed = e.value;
        observed.timestamp = qymera_timestamp_now();
        qymera_event_make_actuator_changed(&ev, device_id, entity_id, &observed);
        qymera_event_bus_publish(context->event_bus, &ev);
    }
}

/* ---- Timeout engine -------------------------------------------------------- */

void qymera_control_tick(qymera_control_context_t *context, uint32_t now_ms) {
    if (!context) return;
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        qymera_pending_command_t *entry = &context->pending[i];
        if (!entry->used) continue;
        if (entry->status == QYMERA_CMD_STATE_CONFIRMED || entry->status == QYMERA_CMD_FAILED) {
            continue;
        }
        if (now_ms >= entry->deadline) {
            uint16_t entity_idx;
            if (qymera_registry_find_entity(context->registry, entry->device_id, entry->entity_id, &entity_idx) == QYMERA_OK) {
                qymera_entity_t e;
                if (qymera_registry_get_entity(context->registry, entity_idx, &e) == QYMERA_OK) {
                    qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_TIMEOUT);
                }
            }
            if (context->log) qymera_log_warn(context->log, "control", "command seq=%lu TIMEOUT (desired kept, observed unchanged)",
                                              (unsigned long)entry->cmd_seq);
            pending_terminal(entry, QYMERA_CMD_TIMEOUT);
        }
    }
}