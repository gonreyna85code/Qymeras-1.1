/**
 * Qymera Dashboard - Control Context + Remote Command State Machine
 *
 * Implementation of the typed control context and the bounded pending-command
 * table that tracks remote relay/dimmer commands from dispatch through
 * ACK/state confirmation (or timeout/failure).
 */
#include "qymera_control.h"
#include "qymera_registry.h"
#include "qymera_udp.h"
#include "qymera_hal.h"
#include "qymera_event_bus.h"
#include "qymera_log.h"
#include <string.h>
#include <stdio.h>

/* ---- Registry helper callbacks (for iterate) --------------------------- */

/* Deterministic FNV-1a hash mapping a dashboard entity_id string onto the
 * uint32 entity_id the UDP protocol uses. Used consistently for command
 * transmission and for matching authoritative state reports back to entities. */
static uint32_t qymera_entity_id_hash(const char *id) {
    uint32_t h = 2166136261u;
    if (!id) return h;
    while (*id) {
        h ^= (uint8_t)*id++;
        h *= 16777619u;
    }
    return h;
}

typedef struct {
    const char *ip;
    uint16_t device_idx;
    bool found;
} qymera_find_by_ip_ctx_t;

static bool find_device_by_ip(uint16_t idx, const qymera_device_t *device, void *context) {
    qymera_find_by_ip_ctx_t *ctx = (qymera_find_by_ip_ctx_t *)context;
    if (device->ip_addr[0] != '\0' && strcmp(device->ip_addr, ctx->ip) == 0) {
        ctx->device_idx = idx;
        ctx->found = true;
        return false; /* stop */
    }
    return true; /* continue */
}

/* ---- Control context lifecycle ------------------------------------------ */

qymera_err_t qymera_control_context_init(qymera_control_context_t *context,
                                          qymera_registry_t *registry,
                                          qymera_udp_transport_t *udp,
                                          qymera_event_bus_t *event_bus,
                                          qymera_log_t *log) {
    if (!context) return QYMERA_ERR_INVALID_ARG;
    memset(context, 0, sizeof(*context));
    context->registry = registry;
    context->udp = udp;
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

static qymera_pending_command_t *pending_find_by_seq(qymera_control_context_t *context, uint32_t cmd_seq) {
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        if (context->pending[i].used && context->pending[i].cmd_seq == cmd_seq) {
            return &context->pending[i];
        }
    }
    return NULL;
}

static void pending_terminal(qymera_pending_command_t *entry, qymera_cmd_status_t status) {
    entry->status = status;
    entry->used = false; /* terminal state frees the slot */
}

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

    /* Remote device: bounded pending dispatch */
    if (!context->udp) return QYMERA_ERR_NETWORK;

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
    entry->used = true;

    uint32_t sent_seq = 0;
    qymera_err_t send_err = qymera_udp_transport_send_command(
        context->udp, device.ip_addr, qymera_entity_id_hash(entity.entity_id), 1, entry->requested_value, 0, &sent_seq);
    if (send_err != QYMERA_OK) {
        /* Dispatch failed -> terminal FAILED, no zombie entry */
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        return send_err;
    }

    entry->cmd_seq = sent_seq;
    entry->status = QYMERA_CMD_WAITING_ACK;

    /* Requested state recorded; observed state unchanged */
    desired.reliability = QYMERA_RELIABILITY_PENDING;
    qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_WAITING_ACK);

    if (context->log) qymera_log_action(context->log, "control", "relay %s -> %s dispatched seq=%lu (remote %s)",
                                        entity_ref->entity_id, state ? "ON" : "OFF",
                                        (unsigned long)sent_seq, device.ip_addr);

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

    if (!context->udp) return QYMERA_ERR_NETWORK;

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
    entry->used = true;

    uint32_t sent_seq = 0;
    qymera_err_t send_err = qymera_udp_transport_send_command(
        context->udp, device.ip_addr, qymera_entity_id_hash(entity.entity_id), 2, (float)level, 0, &sent_seq);
    if (send_err != QYMERA_OK) {
        pending_terminal(entry, QYMERA_CMD_FAILED);
        qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_FAILED);
        return send_err;
    }

    entry->cmd_seq = sent_seq;
    entry->status = QYMERA_CMD_WAITING_ACK;

    desired.reliability = QYMERA_RELIABILITY_PENDING;
    qymera_registry_set_entity_desired(context->registry, entity_idx, &desired, QYMERA_CMD_WAITING_ACK);

    if (context->log) qymera_log_action(context->log, "control", "dimmer %s -> %u dispatched seq=%lu (remote %s)",
                                        entity_ref->entity_id, level, (unsigned long)sent_seq, device.ip_addr);
    return QYMERA_OK;
}

/* ---- ACK handling --------------------------------------------------------- */

void qymera_control_on_ack(qymera_control_context_t *context,
                           uint32_t ack_cmd_seq, uint8_t ack_result,
                           const char *src_ip) {
    if (!context) return;

    qymera_pending_command_t *entry = pending_find_by_seq(context, ack_cmd_seq);
    if (!entry) {
        /* Unknown / late / duplicate already-resolved cmd_seq: ignore safely */
        if (context->log) qymera_log_debug(context->log, "control", "ACK seq=%lu ignored (unknown/late)", (unsigned long)ack_cmd_seq);
        return;
    }

    /* Source validation: incoming ACK must come from the device we sent to */
    if (src_ip && entry->dest_ip[0] != '\0') {
        if (strcmp(src_ip, entry->dest_ip) != 0) {
            if (context->log) qymera_log_warn(context->log, "control", "ACK seq=%lu wrong source %s (expected %s) ignored",
                                              (unsigned long)ack_cmd_seq, src_ip, entry->dest_ip);
            return; /* do not resolve another node's command */
        }
    }

    /* ACK carries an error/result flag (1 = error) */
    if (ack_result != 0) {
        if (context->log) qymera_log_warn(context->log, "control", "ACK seq=%lu result=ERROR -> FAILED", (unsigned long)ack_cmd_seq);
        uint16_t entity_idx;
        if (qymera_registry_find_entity(context->registry, entry->device_id, entry->entity_id, &entity_idx) == QYMERA_OK) {
            qymera_entity_t e;
            if (qymera_registry_get_entity(context->registry, entity_idx, &e) == QYMERA_OK) {
                qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_FAILED);
            }
        }
        pending_terminal(entry, QYMERA_CMD_FAILED);
        return;
    }

    /* First ACK: accepted. Second/duplicate ACK finds the entry already gone
     * (freed on terminal), so it falls into the "ignored" branch above. */
    entry->status = QYMERA_CMD_ACKED;

    uint16_t entity_idx;
    if (qymera_registry_find_entity(context->registry, entry->device_id, entry->entity_id, &entity_idx) == QYMERA_OK) {
        qymera_entity_t e;
        if (qymera_registry_get_entity(context->registry, entity_idx, &e) == QYMERA_OK) {
            qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_ACKED);
        }
    }

    if (context->log) qymera_log_event(context->log, "control", "ACK seq=%lu from %s -> ACKED (not confirmed)", (unsigned long)ack_cmd_seq, src_ip ? src_ip : "?");
    /* IMPORTANT: ACKED != CONFIRMED. The entry is left in WAITING/ACKED so the
     * pending state report can confirm (or the timeout can expire it). */
}

/* ---- Remote state report handling ----------------------------------------- */

void qymera_control_on_state(qymera_control_context_t *context,
                             uint32_t entity_id, uint8_t entity_type,
                             const float *value_f, const uint32_t *value_u32,
                             bool has_float, bool has_u32,
                             const char *src_ip) {
    if (!context || !src_ip) return;

    /* Resolve the reporting device by source IP */
    qymera_find_by_ip_ctx_t ip_ctx = { .ip = src_ip, .found = false };
    qymera_registry_iterate_devices(context->registry, find_device_by_ip, &ip_ctx);
    if (!ip_ctx.found) {
        if (context->log) qymera_log_debug(context->log, "control", "state report from %s: device unknown", src_ip);
        return;
    }
    const char *device_id = NULL;
    {
        qymera_device_t d;
        if (qymera_registry_get_device(context->registry, ip_ctx.device_idx, &d) == QYMERA_OK) {
            device_id = d.device_id;
        }
    }
    if (!device_id) return;

    /* entity_id in the report is the uint32 hash of the dashboard entity_id
     * string (same mapping used when transmitting the command). Locate the
     * actuator entity on that device by matching the hash. */
    uint16_t entity_idx;
    {
        entity_idx = 0;
        bool matched_ent = false;
        size_t max_entities = QYMERA_MAX_ENTITIES;
        for (size_t i = 0; i < max_entities; i++) {
            qymera_entity_t ent;
            if (qymera_registry_get_entity(context->registry, (uint16_t)i, &ent) != QYMERA_OK) continue;
            if (strcmp(ent.device_id, device_id) != 0) continue;
            if (qymera_entity_id_hash(ent.entity_id) == entity_id) {
                entity_idx = (uint16_t)i;
                matched_ent = true;
                break;
            }
        }
        if (!matched_ent) {
            if (context->log) qymera_log_debug(context->log, "control", "state report entity hash %lu not found on %s",
                                               (unsigned long)entity_id, device_id);
            return;
        }
    }

    qymera_entity_t e;
    if (qymera_registry_get_entity(context->registry, entity_idx, &e) != QYMERA_OK) return;

    /* Build authoritative observed state */
    qymera_entity_value_t observed = {0};
    observed.valid = true;
    observed.timestamp = qymera_timestamp_now();
    observed.reliability = QYMERA_RELIABILITY_CONFIRMED;
    if (has_float) {
        observed.numeric_value = *value_f;
        observed.bool_value = (*value_f != 0.0f);
    } else if (has_u32) {
        observed.numeric_value = (float)*value_u32;
        observed.bool_value = (*value_u32 != 0);
    } else {
        return;
    }

    /* Match desired vs observed */
    bool matched = false;
    if (e.desired.valid) {
        bool desired_val = (e.desired.bool_value) || (e.desired.numeric_value != 0.0f);
        if (desired_val == observed.bool_value) {
            matched = true;
        }
    }

    qymera_registry_update_entity_value(context->registry, entity_idx, &observed);

    /* Resolve a pending command whose requested state matches reported state */
    for (int i = 0; i < QYMERA_MAX_PENDING_COMMANDS; i++) {
        qymera_pending_command_t *entry = &context->pending[i];
        if (!entry->used) continue;
        if (strcmp(entry->device_id, device_id) != 0) continue;
        if (strcmp(entry->entity_id, e.entity_id) != 0) continue;
        if (entry->desired_bool == observed.bool_value || entry->desired_numeric == observed.numeric_value) {
            qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_STATE_CONFIRMED);
            pending_terminal(entry, QYMERA_CMD_STATE_CONFIRMED);
        } else {
            /* Mismatch: desired != observed. Represent without overwriting. */
            qymera_registry_set_entity_desired(context->registry, entity_idx, &e.desired, QYMERA_CMD_FAILED);
            pending_terminal(entry, QYMERA_CMD_FAILED);
        }
    }

    if (context->event_bus) {
        qymera_event_t ev;
        qymera_event_make_actuator_changed(&ev, device_id, e.entity_id, &observed);
        qymera_event_bus_publish(context->event_bus, &ev);
    }
    if (context->log) qymera_log_event(context->log, "control", "state %s/%s observed=%.1f matched=%s",
                                       device_id, e.entity_id, observed.numeric_value, matched ? "yes" : "no");
    (void)entity_type;
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
